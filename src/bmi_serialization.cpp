extern "C" {
#include "../include/topmodel.h"
}
#include <cstdint>
#include <cstdio>
#include "../include/bmi_serialization.h"

#include <stdexcept>
#include <vector>

#include <boost/serialization/serialization.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "../include/vecbuf.hpp"


class TopmodelSerializer {
    public:
        TopmodelSerializer(Bmi* bmi)
            : model((topmodel_model*)bmi->data) {};
        ~TopmodelSerializer() = default;

    private:
        friend class boost::serialization::access;
        topmodel_model* model;
        template<class Archive>
        void serialize(Archive& ar, const unsigned int version);
};

/* Bump on any change to what serialize() archives, so that older payloads are
   rejected rather than misread.  Boost stores this in the archive itself. */
#define TOPMODEL_SERIALIZATION_LAYOUT_VERSION 1
BOOST_CLASS_VERSION(TopmodelSerializer, TOPMODEL_SERIALIZATION_LAYOUT_VERSION)

/*
 * Archive a field in both directions, applying an incoming value only when it
 * belongs to the run doing the restoring.  The value is always written and always
 * read, so the byte layout does not depend on what is applied.
 */
template<class Archive, class T>
static void archive_field(Archive& ar, T& value, bool apply) {
    if (Archive::is_saving::value) {
        ar & value;
    } else {
        T incoming;
        ar & incoming;
        if (apply)
            value = incoming;
    }
}

template<class Archive>
static void archive_array_field(Archive& ar, double* values, int count, bool apply) {
    if (Archive::is_saving::value) {
        ar & boost::serialization::make_array(values, count);
    } else {
        std::vector<double> incoming(count);
        ar & boost::serialization::make_array(incoming.data(), count);
        if (apply)
            memcpy(values, incoming.data(), (size_t)count * sizeof(double));
    }
}


template<class Archive>
void TopmodelSerializer::serialize(Archive& ar, const unsigned int version) {
    topmodel_model* model = this->model;
    // Check before archiving anything, so a rejected payload leaves state untouched
    if (Archive::is_loading::value && version != TOPMODEL_SERIALIZATION_LAYOUT_VERSION) {
        char error[128];
        snprintf(error, sizeof(error),
                 "state is layout version %u, but this build expects %d",
                 version, TOPMODEL_SERIALIZATION_LAYOUT_VERSION);
        throw std::runtime_error(error);
    }
    if (model->stand_alone == TRUE) {
        // the number of timesteps makes hindcasting nigh imposible when stand alone
        auto error = "Topmodel serialization is not currently implemented when running stand alone.";
        fprintf(stderr, "%s\n", error);
        throw std::runtime_error(error);
    }
    // The clock and the totals below describe the run that wrote the snapshot,
    // so they carry over only when this run is continuing it.
    const bool resume = model->restore_mode == TOPMODEL_RESTORE_RESUME;

    archive_field(ar, model->current_time_step, resume);

    // data summed between runs
    archive_field(ar, model->sump, resume);
    archive_field(ar, model->sumae, resume);
    archive_field(ar, model->sumq, resume);
    archive_field(ar, model->sumrz, resume); // reassigned each update; not used for calcs
    archive_field(ar, model->sumuz, resume); // reassigned each update; not used for calcs

    // outputs of Update
    ar & model->Qout; // reassigned each update; not used for calcs
    ar & model->quz; //
    ar & model->qb; // reassigned each update; not used for calcs
    ar & model->qof; //
    ar & model->p; // reassigned each update; used in calc after assignment
    ar & model->ep; // reassigned each update; used in calc after assignment
    ar & model->sbar; // used then reassigned

    // array data that updates in update; counts set in config
    // these have an actual size 1 larger than the number
    int num_topodex_values = model->num_topodex_values + 1;
    ar & boost::serialization::make_array(
        model->deficit_root_zone, num_topodex_values
    );
    ar & boost::serialization::make_array(
        model->stor_unsat_zone, num_topodex_values
    );
    ar & boost::serialization::make_array(
        model->deficit_local, num_topodex_values
    );

    // nsteps will always be 1 for non-stand-alone models
    ar & boost::serialization::make_array(
        model->contrib_area, model->nstep + 1
    );

    // Channel routing counts come from config, so this run's own values stand.
    // A snapshot that disagrees describes a different catchment setup and is
    // refused rather than resized into.
    int num_time_delay_histo_ords = model->num_time_delay_histo_ords;
    ar & num_time_delay_histo_ords;
    int num_delay = model->num_delay;
    ar & num_delay;
    if (Archive::is_loading::value
        && (num_time_delay_histo_ords != model->num_time_delay_histo_ords
            || num_delay != model->num_delay)) {
        char error[160];
        snprintf(error, sizeof(error),
                 "state has %d histogram ordinates and %d delay steps, but this "
                 "configuration has %d and %d",
                 num_time_delay_histo_ords, num_delay,
                 model->num_time_delay_histo_ords, model->num_delay);
        throw std::runtime_error(error);
    }

    // The weights come from config too, and the counts matching does not mean the
    // weights do, so this run's own stand under either mode.
    archive_array_field(ar, model->time_delay_histogram, model->num_time_delay_histo_ords + 1, false);

    size_t num_Q = model->num_delay + model->num_time_delay_histo_ords + 1;
    ar & boost::serialization::make_array(model->Q, num_Q);
}


extern "C" {

/**
 * Serializes a Topmodel BMI model through boost. Formats the data as binary output for smaller memory impact than text.
 * It is the responsibility of the caller to free the newly allocated memory if BMI_SUCCESS is returned.
 * 
 * @param bmi topmodel BMI model that will be serialized
 * @param buffer Pointer to a char pointer. The pointer's pointer will be assigned to the serialized data.
 * @param size_written Pointer to the amount of data that was written to the buffer.
 * @return int signifiying whether the serialization process completed successfully.
 */
const int serialize_topmodel(Bmi* bmi) {
    TopmodelSerializer serializer(bmi);
    vecbuf<char> stream;
    try {
        // Constructing the archive writes the header, so it belongs inside the try
        boost::archive::binary_oarchive archive(stream);
        archive << serializer;
    } catch (const std::exception& e) {
        fprintf(stderr, "Serializing Topmodel encountered an error: %s\n", e.what());
        return BMI_FAILURE;
    }
    // copy serialized data into topmodel data
    topmodel_model* model = (topmodel_model*)bmi->data;
    // clear previous data if it exists
    if (model->serialized != NULL) {
        free(model->serialized);
    }
    // set size and allocate memory
    uint64_t serialized_size = stream.size();
    model->serialized_length = (int)(serialized_size + sizeof(uint64_t));
    model->serialized = (char*)malloc(model->serialized_length);
    // make sure memory could be allocated
    if (model->serialized == NULL) {
        model->serialized_length = 0;
        return BMI_FAILURE;
    }
    // copy stream data to new allocation
    memcpy(model->serialized, &serialized_size, sizeof(uint64_t));
    memcpy(model->serialized + sizeof(uint64_t), stream.data(), serialized_size);
    return BMI_SUCCESS;
}

 /**
  * Deserializes data into a Topmodel BMI model.
  * 
  * @param bmi Topmodel BMI model that will have values inserted into it.
  * @param buffer Start of data that wil be read as previously serialized state
  * @return int signifiying whether the serialization process completed successfully.
  */
const int deserialize_topmodel(Bmi* bmi, char* buffer) {
    TopmodelSerializer serializer(bmi);
    // copy size of data out of header
    uint64_t size;
    memcpy(&size, buffer, sizeof(uint64_t));
    // create stream from data after header
    membuf stream(buffer + sizeof(uint64_t), size);
    try {
        // Constructing the archive validates the header, so it belongs inside the
        // try; otherwise a foreign or corrupt payload escapes as an exception
        boost::archive::binary_iarchive archive(stream);
        archive >> serializer;
        return BMI_SUCCESS;
    } catch (const std::exception &e) {
        fprintf(stderr, "Deserializing Topmodel encountered an error: %s\n", e.what());
        return BMI_FAILURE;
    }
}

}
