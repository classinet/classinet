//
// Created by yaniv on 9/1/22.
//

// Note: All API is thread safe.

#ifndef CLASSINET_CLIENT_CPP_API_H
#define CLASSINET_CLIENT_CPP_API_H

#include <memory>
#include <mutex>
#include <map>
#include <vector>
#include <functional>


struct image_object_detection_model_metadata : public std::enable_shared_from_this<image_object_detection_model_metadata> {
    std::string name;
    std::string io_wrapper;
    int input_width, input_height;
    //std::map<int, float> thresholds;
    std::map<int, std::string> class_ids;
    std::string model_id; // ignored when registering, given by classinet when registering a model.
    std::string description;

    std::string ToString();

    void SetFromString(const std::string &metadata_string);
};

namespace classinet {
    static std::vector<std::string> tokenize(const std::string &s, const std::string &delim) {
        if (s.empty())
            return {};
        std::vector<std::string> tokens;
        auto start = 0U;
        auto end = s.find(delim);
        while (end != std::string::npos) {
            tokens.push_back(s.substr(start, end - start));
            start = end + delim.length();
            end = s.find(delim, start);
        }
        tokens.push_back(s.substr(start));
        return tokens;
    };

    struct debug_hints {
        static std::shared_ptr<debug_hints> Singleton(const std::string& hints_string = {}) {
            static auto singleton = std::make_shared<debug_hints>();
            if(!hints_string.empty())
                singleton->Parse(hints_string);
            return singleton;
        }
        void Parse(const std::string& hints_string);
        static const std::string Get(const std::string& field) {
            auto singleton = Singleton();
            return singleton->hints[field];
        }
        std::map<std::string, std::string> hints;
    };

    struct image_object_detection_model;

    struct client : public std::enable_shared_from_this<client> {

        // Will return a null if classinet was not connected. Use Connect() first.
        static std::shared_ptr<client> Get() {
            return the_client;
        };

        static std::shared_ptr<client> Connect(const std::string &user_token, const std::string &instance_description = {});;

        const std::string &GetUserToken() const {
            return user_token;
        }

        const std::string &GetInstanceDescription() const {
            return instance_description;
        }

        const std::string GetState() const;

        std::vector<image_object_detection_model_metadata> GetAvailableModels();

        std::shared_ptr<image_object_detection_model> RegisterModel(const image_object_detection_model_metadata &metadata, const std::string &binary_model);

        // note model_id is an id given by classinet to a registered model. It may be obtained from the respective field in the model metadata.
        std::shared_ptr<image_object_detection_model> GetModel(const std::string &model_id);


        static const std::string GetVersion();

        static const std::string Encode(const std::string &plain);

        static const std::string Decode(const std::string &cipher);

    private:
        explicit client(const std::string &user_token, const std::string &instance_description = {}) :
                user_token(user_token), instance_description(instance_description), instance_id(make_instance_id()) {
        };

        std::string user_token;
        std::string instance_description;
        std::string instance_id;

        std::mutex mutex;

        static std::shared_ptr<client> the_client;

        std::string make_instance_id();;

        void Startup(const std::string &user_token, const std::string &instance_description = {});

    };

    struct image_object_detection_model : public std::enable_shared_from_this<image_object_detection_model> {
        // image parameters is the binary content of an image.
        // user_context is optional user supplied description of this request for the purpose of billing records and diagnostics. Limited to 128 ascii chars.
        std::string Infer(const std::string &image, const std::string &user_context = {});

        typedef std::function<void(const std::string& inference)> InferenceCallback;

        void AsyncInfer(const std::string &image, InferenceCallback callback, const std::string &user_context = {});

    private:
        image_object_detection_model_metadata metadata;

        explicit image_object_detection_model(image_object_detection_model_metadata &metadata) : metadata(metadata) {};

        friend client;

        static std::shared_ptr<image_object_detection_model> Make(image_object_detection_model_metadata &metadata);

    };

}

#endif //CLASSINET_CLIENT_CPP_API_H
