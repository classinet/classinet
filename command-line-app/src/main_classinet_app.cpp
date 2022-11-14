// Normally you don't both the c and c++ api. Here we just demo their usage
#include "classinet_client_cpp_api.h"
#include "classinet_client_c_api.h"

#include <string>
#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <future>

#include "LinuxSpecific.h"


std::string help_text() {
    std::string help = R"END(Classinet client command line application. See https://classinet.com
Copyright (c) 2022 Classinet Technologies LTD. License: https:://classinet.com/license
Version $version$

Usage:
classinet -h|--help|help
classinet -c|--configure|configure [(--user_token|-u) token] [(--instance_description|-d) description] [(--debug|-g) debug_options]
classinet -l|--list|list [(--model|-m) model_name] [CONTEXT]
classinet -u|--upload|upload (--metadata|-d) metadata_filename (--model|-m) model_filename [CONTEXT]
classinet -i|--infer|infer (--model|-m) model_name [CONTEXT] [(--txt|-t) output_txt_annotation_path]  [(--marked|-k) output_marked_images_path] images images ...

help:
Print this help text

configure:
Stores the parameter values encrypted in $config_file$. Further commands read and use those values as default.
user_token is obtained at https://classinet.com - it represent the registered classinet user enabling consumption of resources.
Keep user_token private as it enables billing the user for classinet resources used.
instance_description is optional and describes this instance of classinet client, enabling the user to track actions from this instance in billing reports and logs.
debug_options are for internal testing. Don't use unless instructed by classinet.
The special value "clear" deletes the configuration option. For example "--user_token clear" would remove the default user token from the configuration file.

list:
List all models available to this user with their metadata.

upload:
Upload a machine learning model to the classinet cloud. For the format of the metadata file see https://classinet.com/docs/model_metadata
At this version only binary images of TensorRT models are supported. Other model formats are in the works.

infer:
Make inference using the specified model for the images specified at the end of the parameter list. Wildcards and directories are supported.
Output is written to stdout.
When --txt is specified darknet style text files with the detections are created for each input image.
When --marked is specified new image files are created there for each input image with the detections marked in the image.

[CONTEXT] represents all options of the configure command. By default the values for these options is obtained from the saved configuration file.
Specifying such option in the command line overrides the default. One way or another user_token must always be resolved to a valid value for all command.
)END";

    auto version = classinet::client::GetVersion();
    std::string version_holder{"$version$"};
    help.replace(help.find(version_holder), version_holder.length(), version);
    std::string config_file_holder{"$config_file$"};
    help.replace(help.find(config_file_holder), config_file_holder.length(), OS::classinet_config_file());

    return help;
}

std::map<std::string, std::string> read_config_file() {
    std::ifstream stream(OS::classinet_config_file(), std::ios::in | std::ios::binary);
    if (!stream.good())
        return {};
    std::string cipher((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    auto s = classinet::client::Decode(cipher);

    auto lines = classinet::tokenize(s, "\n");
    std::map<std::string, std::string> ret;
    for (const auto &line: lines) {
        auto parts = classinet::tokenize(line, ": ");
        if (parts.size() < 2)
            continue;
        if (parts.size() > 2) // can happen when there is ": " in value
            parts[1] = line.substr(parts[0].length() + 2);
        ret[parts[0]] = parts[1];
    }

    return ret;
}

void save_config_file(const std::map<std::string, std::string> &arguments) {
    std::stringstream ss;
    for (const auto &arg: arguments)
        ss << arg.first << ": " << arg.second << "\n";
    auto cipher = classinet::client::Encode(ss.str());

    try {
        if (!std::filesystem::is_directory(OS::classinet_config_dir()))
            std::filesystem::create_directory(OS::classinet_config_dir());
        std::ofstream out(OS::classinet_config_file(), std::ofstream::binary);
        out << cipher;
        out.close();
    } catch (const std::exception &e) {
        std::cout << "Unable to save config file at " << OS::classinet_config_file() << " " << e.what() << "- continuing\n";
    }
}

void save_configuration(const std::map<std::string, std::string> &arguments) {
    // if no context args on command line then nothing to do.
    if (arguments.empty())
        return;

    auto config = read_config_file();
    for (const auto &arg: arguments) {
        if ("clear" == arg.second) {
            auto f = config.find(arg.first);
            if (config.end() != f)
                config.erase(f);
        } else if (!arg.second.empty()) {
            config[arg.first] = arg.second;
        }
    }

    save_config_file(config);
}

void add_default_configuration(std::map<std::string, std::string> &arguments) {
    auto config = read_config_file();
    for (const auto &arg: config) {
        auto f = arguments.find(arg.first);
        if (arguments.end() == f || f->second.empty())
            arguments[arg.first] = arg.second;
    }
}

void upload_model(const std::map<std::string, std::string> &arguments) {
}

void list_models(const std::map<std::string, std::string> &arguments) {
}

// some help C code to assist in Async C API test inference getting context
struct C_single_inference_context {
    std::chrono::steady_clock::time_point start_time;
};

struct C_inference_context {
    int real_files;
    int total_latency_real_files;
    int pending;
    int total_files_originally;
    std::mutex mutex;
    std::function<void(void)> finally;
    std::map<std::string, C_single_inference_context> in_progress;
};

static auto c_contexts = std::make_shared<C_inference_context>();

void inference(std::map<std::string, std::string> &arguments, const std::vector<std::string> &patterns) {
    classinet::debug_hints::Singleton(arguments["debug"]);

    std::vector<std::string> files;
    for (const auto &pattern: patterns) {
        auto n = OS::expand_wildcard(pattern);
        if (n.empty())
            std::cout << "Ignoring not found files: " + pattern + "\n";
        for (const auto &s: n)
            if (!s.empty() && (std::filesystem::is_regular_file(s) || std::filesystem::is_symlink(s)))
                files.push_back(s);
    }

    if (files.empty()) {
        std::cout << "No valid file in parameter list. Aborting.\n";
        exit(1);
    }


//    std::cout << "options:::\n";
//    for (const auto &arg: arguments)
//        std::cout << arg.first << ": " << arg.second << "\n";
//    std::cout << "files:\n";
//    for (const auto &file: files)
//        std::cout << file << "\n";

    auto api = classinet::debug_hints::Get("api");

    if ("c++" == api || "cpp" == api || "a_cpp" == api || "acpp" == api || api.empty()) {
        // Asynchronous C++ API variant

        auto cc = classinet::client::Connect(arguments["user_token"], arguments["instance_description"]);
        if ("ready" != cc->GetState()) {
            std::cout << "Failed to activate classinet client: " << cc->GetState() << "\n";
            std::cout << "Aborting.\n";
            exit(1);
        }

        auto model = cc->GetModel(arguments["model"]);
        if (!model) {
            std::cout << "Model unavailable. Aborting.\n";
            exit(1);
        }

        auto all_start_time = std::chrono::steady_clock::now();

        int real_files{0};
        int total_latency_real_files{0};
        std::promise<bool> done_promise;
        std::future<bool> done_future = done_promise.get_future();
        static auto mutex = std::make_shared<std::mutex>();
        auto pending = std::make_shared<int>(files.size());
        for (const auto file: files) {
            std::ifstream stream(file, std::ios::in | std::ios::binary);
            if (!stream.good()) {
                std::lock_guard<std::mutex> lock(*mutex);
                std::stringstream ss;
                if ("yes" == classinet::debug_hints::Get("stats"))
                    ss << "latency: 0ms;";
                ss << "file: " << file << ";";
                ss << " error: unable to read file;\n";
                std::cout << ss.str();
                --*pending;
                if (*pending <= 0)
                    done_promise.set_value(true);
                continue;
            }
            std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

            auto start_time = std::chrono::steady_clock::now();
            model->AsyncInfer(contents, [file, start_time, mutex = mutex, pending, &done_promise, &real_files, &total_latency_real_files](const std::string &inference) {
                auto latency = std::chrono::steady_clock::now() - start_time;
                auto milliseconds = int(std::chrono::duration_cast<std::chrono::milliseconds>(latency).count());
                std::lock_guard<std::mutex> lock(*mutex); // writing lines to std::cout is not thread safe.
                ++real_files;
                total_latency_real_files += milliseconds;
                std::stringstream ss;
                if ("yes" == classinet::debug_hints::Get("stats"))
                    ss << "latency: " << milliseconds << "ms; ";
                ss << "file: " << file << ";";
                ss << " " << inference << "\n";
                std::cout << ss.str();
                --*pending;
                if (*pending <= 0)
                    done_promise.set_value(true);
            });
        }
        done_future.get();

        auto all_latency = std::chrono::steady_clock::now() - all_start_time;
        auto milliseconds = int(std::chrono::duration_cast<std::chrono::milliseconds>(all_latency).count());
        if ("yes" == classinet::debug_hints::Get("stats")) {
            std::cout << "total time: " << milliseconds << "ms; number of files: " << files.size() << "; rate: " << 1000 * files.size() / milliseconds << " images/sec;\n";
            if (0 == real_files)
                real_files = 1;
            std::cout << "average latency on real files: " << total_latency_real_files / real_files << "ms;\n";
        }

    } else if ("s_c++" == api || "s_cpp" == api || "scpp" == api) {
        // Synchronous C++ API variant

        auto cc = classinet::client::Connect(arguments["user_token"], arguments["instance_description"]);
        if ("ready" != cc->GetState()) {
            std::cout << "Failed to activate classinet client: " << cc->GetState() << "\n";
            std::cout << "Aborting.\n";
            exit(1);
        }

        auto model = cc->GetModel(arguments["model"]);
        if (!model) {
            std::cout << "Model unavailable. Aborting.\n";
            exit(1);
        }

        auto all_start_time = std::chrono::steady_clock::now();

        int real_files{0};
        int total_latency_real_files{0};
        for (const auto file: files) {
            std::ifstream stream(file, std::ios::in | std::ios::binary);
            if (!stream.good()) {
                std::stringstream ss;
                if ("yes" == classinet::debug_hints::Get("stats"))
                    ss << "latency: 0ms;";
                ss << "file: " << file << ";";
                ss << " error: unable to read file;\n";
                std::cout << ss.str();
                continue;
            }
            std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

            auto start_time = std::chrono::steady_clock::now();
            auto inference = model->Infer(contents);
            auto latency = std::chrono::steady_clock::now() - start_time;
            auto milliseconds = int(std::chrono::duration_cast<std::chrono::milliseconds>(latency).count());
            ++real_files;
            total_latency_real_files += milliseconds;
            std::stringstream ss;
            if ("yes" == classinet::debug_hints::Get("stats"))
                ss << "latency: " << milliseconds << "ms; ";
            ss << "file: " << file << ";";
            // note: for synchronous infer with error the inference contains the error.
            ss << " " << inference << "\n";
            std::cout << ss.str();
        }

        auto all_latency = std::chrono::steady_clock::now() - all_start_time;
        auto milliseconds = int(std::chrono::duration_cast<std::chrono::milliseconds>(all_latency).count());
        if ("yes" == classinet::debug_hints::Get("stats")) {
            std::cout << "total time: " << milliseconds << "ms; number of files: " << files.size() << "; rate: " << 1000 * files.size() / milliseconds << " images/sec;\n";
            if (0 == real_files)
                real_files = 1;
            std::cout << "average latency on real files: " << total_latency_real_files / real_files << "ms;\n";
        }
    } else if ("ac" == api) {
        // Asynchronous C API variant

        auto cc = classinet_client_connect(arguments["user_token"].c_str(), arguments["instance_description"].c_str());
        char status[CLASSINET_STRING_VALUE_LENGTH];
        auto st = classinet_client_status(status);
        if (0 == cc || 0 == st) {
            std::cout << "Failed to activate classinet client: " << std::string{status} << "\n";
            std::cout << "Aborting.\n";
            exit(1);
        }

        // we could have skipped this section for this demo because we are not really using the model metadata, but this way we know if model is available before doing any inference
        std::string metadata;
        // try buffer lengths.
        for (size_t size = 1024; size < 256 * 256; size *= 2) {
            metadata = std::string(size, '\0');
            size_t metadata_buffer_length = size;
            auto md = classinet_get_model_metadata(arguments["model"].c_str(), &metadata[0], &metadata_buffer_length);
            if (0 == md) {
                metadata.clear();
                break;
            }
            if (metadata_buffer_length == size)
                continue;
            metadata.resize(metadata_buffer_length);
        }
        if (metadata.empty()) {
            std::cout << "Model unavailable. Aborting.\n";
            exit(1);
        }

        auto all_start_time = std::chrono::steady_clock::now();

        std::promise<void> promise;
        std::future<void> future = promise.get_future();

        // Apologies: C callback code needs a lot of boilerplate code. Callback is a different C function. Need to store and recover the context of the original request.
        // A different C function for when all is done, and a way to wait for it to get done.
        // Doing all that in C would spoil the neatness of this code. So instead I store the context and everything I need in a C++ global structure and use lambda.
        // But under all this boilerplate it's the C API doing the classinet work.

        c_contexts->real_files = 0;
        c_contexts->total_latency_real_files = 0;
        c_contexts->pending = files.size();
        c_contexts->total_files_originally = files.size();
        c_contexts->finally = [all_start_time, &promise]() {
            auto all_latency = std::chrono::steady_clock::now() - all_start_time;
            auto milliseconds = int(std::chrono::duration_cast<std::chrono::milliseconds>(all_latency).count());
            if ("yes" == classinet::debug_hints::Get("stats")) {
                std::cout << "total time: " << milliseconds << "ms; number of files: " << c_contexts->total_files_originally << "; rate: " << 1000 * c_contexts->total_files_originally / milliseconds << " images/sec;\n";
                if (0 == c_contexts->real_files)
                    c_contexts->real_files = 1;
                std::cout << "average latency on real files: " << c_contexts->total_latency_real_files / c_contexts->real_files << "ms;\n";
            }
            promise.set_value();
        };

        for (const auto file: files) {
            std::ifstream stream(file, std::ios::in | std::ios::binary);
            if (!stream.good()) {
                std::lock_guard<std::mutex> lock(c_contexts->mutex);
                std::stringstream ss;
                if ("yes" == classinet::debug_hints::Get("stats"))
                    ss << "latency: 0ms;";
                ss << "file: " << file << ";";
                ss << " error: unable to read file;\n";
                std::cout << ss.str();
                --c_contexts->pending;
                if (c_contexts->pending <= 0)
                    c_contexts->finally();
                continue;
            }
            std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

            {
                std::lock_guard<std::mutex> lock(c_contexts->mutex);
                c_contexts->in_progress[file].start_time = std::chrono::steady_clock::now();
            }
            // in pure C code we need to write a different function for callback, but for this demo we can use a lambda without caption
            classinet_async_infer(arguments["model"].c_str(), contents.c_str(), contents.length(), NULL, file.c_str(), [](const void *inference_id, const char *inference) {
                auto latency = std::chrono::steady_clock::now() - c_contexts->in_progress[std::string{(char *)inference_id}].start_time;
                auto milliseconds = int(std::chrono::duration_cast<std::chrono::milliseconds>(latency).count());
                std::lock_guard<std::mutex> lock(c_contexts->mutex); // writing lines to std::cout is not thread safe.
                ++c_contexts->real_files;
                c_contexts->total_latency_real_files += milliseconds;
                std::stringstream ss;
                if ("yes" == classinet::debug_hints::Get("stats"))
                    ss << "latency: " << milliseconds << "ms; ";
                ss << "file: " << inference_id << ";";
                ss << " " << inference << "\n";
                std::cout << ss.str();
                --c_contexts->pending;
                if (c_contexts->pending <= 0)
                    c_contexts->finally();
            });
        }
        future.get();
    } else if ("sc" == api) {
        // Asynchronous C API variant

        auto cc = classinet_client_connect(arguments["user_token"].c_str(), arguments["instance_description"].c_str());
        char status[CLASSINET_STRING_VALUE_LENGTH];
        auto st = classinet_client_status(status);
        if (0 == cc || 0 == st) {
            std::cout << "Failed to activate classinet client: " << std::string{status} << "\n";
            std::cout << "Aborting.\n";
            exit(1);
        }

        // we could have skipped this section for this demo because we are not really using the model metadata, but this way we know if model is available before doing any inference
        std::string metadata;
        // try buffer lengths.
        for (size_t size = 1024; size < 256 * 256; size *= 2) {
            metadata = std::string(size, '\0');
            size_t metadata_buffer_length = size;
            auto md = classinet_get_model_metadata(arguments["model"].c_str(), &metadata[0], &metadata_buffer_length);
            if (0 == md) {
                metadata.clear();
                break;
            }
            if (metadata_buffer_length == size)
                continue;
            metadata.resize(metadata_buffer_length);
        }
        if (metadata.empty()) {
            std::cout << "Model unavailable. Aborting.\n";
            exit(1);
        }

        auto all_start_time = std::chrono::steady_clock::now();

        int real_files{0};
        int total_latency_real_files{0};
        for (const auto file: files) {
            std::ifstream stream(file, std::ios::in | std::ios::binary);
            if (!stream.good()) {
                std::stringstream ss;
                if ("yes" == classinet::debug_hints::Get("stats"))
                    ss << "latency: 0ms;";
                ss << "file: " << file << ";";
                ss << " error: unable to read file;\n";
                std::cout << ss.str();
                continue;
            }
            std::string contents((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

            auto start_time = std::chrono::steady_clock::now();

            size_t large_enough = 10240; // guessing this would be enough to hold any inference for now.
            std::string inference(large_enough, '\0');
            // note: as of c++11 std::string is guaranteed to be all in one continuous memory section, and it's legal to take the address of the first char and pretend it's a C string.
            auto r = classinet_infer(arguments["model"].c_str(), contents.c_str(), contents.length(), NULL, &inference[0], &large_enough);
            if (0 == r)
                inference = "error: bad arguments to classinet_infer();";
            else
                inference.resize(large_enough);

            auto latency = std::chrono::steady_clock::now() - start_time;
            auto milliseconds = int(std::chrono::duration_cast<std::chrono::milliseconds>(latency).count());
            ++real_files;
            total_latency_real_files += milliseconds;
            std::stringstream ss;
            if ("yes" == classinet::debug_hints::Get("stats"))
                ss << "latency: " << milliseconds << "ms; ";
            ss << "file: " << file << ";";
            // note: for synchronous infer with error the inference contains the error.
            ss << " " << inference << "\n";
            std::cout << ss.str();
        }

        auto all_latency = std::chrono::steady_clock::now() - all_start_time;
        auto milliseconds = int(std::chrono::duration_cast<std::chrono::milliseconds>(all_latency).count());
        if ("yes" == classinet::debug_hints::Get("stats")) {
            std::cout << "total time: " << milliseconds << "ms; number of files: " << files.size() << "; rate: " << 1000 * files.size() / milliseconds << " images/sec;\n";
            if (0 == real_files)
                real_files = 1;
            std::cout << "average latency on real files: " << total_latency_real_files / real_files << "ms;\n";
        }
    } else {
        std::cout << "Unknown api option: " << arguments["api"] << ". Aborting." << "\n";
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    std::vector<std::string> args;
    for (auto i = 0; i < argc; ++i)
        args.emplace_back(argv[i]);
    // push an empty arg so that we don't have to worry about options not followed by a value argument at the end of the argument line.
    // Note: if ever we add options with more than one parameter we need to push back more empty args here.
    args.push_back({});

    auto in = [](const std::string &what, const std::vector<std::string> &list) -> bool {
        return std::find(std::begin(list), std::end(list), what) != std::end(list);
    };

    auto test_unused = [&args]() {
        bool abort = false;
        for (int i = 2; i < args.size(); ++i)
            if (!args[i].empty()) {
                std::cout << "Misplaced parameter " << args[i] << "\n";
                abort = 1;
            }
        if (abort) {
            std::cout << "Aborting.\nTry: classinet help\n";
            exit(1);
        }
    };

    if (args.size() < 2) {
        std::cout << "No action specified - aborting.\n\n";
        std::cout << help_text();
        return (1);
    }

    std::map<std::string, std::vector<std::string>> context_one_param_args = {
            {"user_token",           {"--user_token",           "-u"}},
            {"instance_description", {"--instance_description", "-i"}},
            {"debug",                {"--debug",                "-g"}},
    };

    if (in(args[1], {"-h", "--help", "help"})) {
        std::cout << help_text();
        return (0);
    } else if (in(args[1], {"-c", "--configure", "configure", "conf", "config"})) {
        std::map<std::string, std::string> parsed;
        auto one_param_args = context_one_param_args;
        for (int i = 2; i < args.size(); ++i)
            for (const auto &option: one_param_args)
                if (in(args[i], option.second)) {
                    parsed[option.first] = args[i + 1];
                    args[i] = {};
                    args[i + 1] = {};
                }
        test_unused();
        save_configuration(parsed);
        return (0);
    } else if (in(args[1], {"-u", "--upload", "upload"})) {
        std::map<std::string, std::string> parsed;
        auto one_param_args = context_one_param_args;
        one_param_args["metadata"] = {"--metadata", "-d"};
        one_param_args["model"] = {"--model", "-m"};
        for (int i = 2; i < args.size(); ++i)
            for (const auto &option: one_param_args)
                if (in(args[i], option.second)) {
                    parsed[option.first] = args[i + 1];
                    args[i] = {};
                    args[i + 1] = {};
                }
        test_unused();
        bool abort = false;
        for (const auto &p: {"metadata", "model"})
            if (parsed[p].empty()) {
                std::cout << "Missing required parameter: " << p << "\n";
                abort = true;
            }
        if (abort) {
            std::cout << "aborting.\nTry: classinet help\n";
            return 1;
        }
        add_default_configuration(parsed);
        upload_model(parsed);
        return (0);
    } else if (in(args[1], {"-l", "--list", "list"})) {
        std::map<std::string, std::string> parsed;
        auto one_param_args = context_one_param_args;
        one_param_args["model"] = {"--model", "-m"};
        for (int i = 2; i < args.size(); ++i)
            for (const auto &option: one_param_args)
                if (in(args[i], option.second)) {
                    parsed[option.first] = args[i + 1];
                    args[i] = {};
                    args[i + 1] = {};
                }
        test_unused();
        add_default_configuration(parsed);
        list_models(parsed);
        return (0);
    } else if (in(args[1], {"-i", "--infer", "infer"})) {
        std::map<std::string, std::string> parsed;
        auto one_param_args = context_one_param_args;
        one_param_args["model"] = {"--model", "-m"};
        one_param_args["txt"] = {"--txt", "-t"};
        one_param_args["marked"] = {"--marked", "-k"};
        for (int i = 2; i < args.size(); ++i)
            for (const auto &option: one_param_args)
                if (in(args[i], option.second)) {
                    parsed[option.first] = args[i + 1];
                    args[i] = {};
                    args[i + 1] = {};
                }
        bool abort = false;
        for (const auto &p: {"model"})
            if (parsed[p].empty()) {
                std::cout << "Missing required parameter: " << p << "\n";
                abort = true;
            }
        if (abort) {
            std::cout << "aborting.\nTry: classinet help\n";
            return 1;
        }
        std::vector<std::string> files;
        args[0] = {};
        args[1] = {};
        for (const auto &a: args)
            if (!a.empty())
                files.push_back(a);
        add_default_configuration(parsed);
        inference(parsed, files);
        return (0);
    } else {
        std::cout << "Unknown action " << args[1] << " - aborting.\nTry: classinet help\n";
        return (1);
    }
}
