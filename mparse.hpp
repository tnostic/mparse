    #pragma once

    #include <iostream>
    #include <string>
    #include <string_view>
    #include <vector>
    #include <functional>
    #include <charconv>
    #include <sstream>
    #include <iomanip>
    #include <utility>
    #include <type_traits>

    namespace mparse {

        template <typename T>
        bool parse_value(std::string_view sv, T& out) {
            if constexpr (std::is_same_v<T, std::string>) {
                out = std::string(sv);
                return true;
            } else if constexpr (std::is_same_v<T, bool>) {
                if (sv == "true" || sv == "1" || sv == "yes") { out = true; return true; }
                if (sv == "false" || sv == "0" || sv == "no") { out = false; return true; }
                return false;
            } else if constexpr (std::is_integral_v<T>) {
                // string to integer conversion
                auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out);
                // check if conversion succeeded
                return (ec == std::errc()) && (ptr == sv.data() + sv.size());
            } else if constexpr (std::is_floating_point_v<T>) {
                // allocation-free float conversion
                auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out);
                return (ec == std::errc()) && (ptr == sv.data() + sv.size());
            } else {
                // fallback
                std::istringstream iss{std::string(sv)};
                return static_cast<bool>(iss >> out);
            }
        }


        // option structs
    struct Option {
        std::string short_name; // e.g: "-f"
        std::string long_name;  // e.g: "--filetype"
        std::string description;
        bool is_flag = false;
        std::function<bool(std::string_view)> callback;
    };

    struct Positional {
        std::string name;
        std::string description;
        std::function<bool(std::string_view)> callback;
        bool matched = false;
    };

    // parser logic
    class parser {
    private:
        std::string program_name_;
        std::string description_;
        std::vector<Option> options_;
        std::vector<Positional> positionals_;
        std::string error_message_;
        bool help_requested_ = false;

    public:
        explicit parser(std::string description = "")
            : description_(std::move(description)) {}

        // Adds a bool flag
        void add_flag(std::string short_name, std::string long_name, bool& target, std::string desc) {
            options_.push_back({
                .short_name = std::move(short_name),
                .long_name = std::move(long_name),
                .description = std::move(desc),
                .is_flag = true,
                .callback = [&target](std::string_view) {
                    target = true;
                    return true;
                }
            });
        }

        // Add an option expecting value
        template <typename T>
        void add_option(std::string short_name, std::string long_name, T& target, std::string desc) {
            options_.push_back({
                std::move(short_name),
                std::move(long_name),
                std::move(desc),
                false,
                [&target](std::string_view sv) {
                    return parse_value(sv, target);
                }
            });
        }

        // Register a positional argument (e.g. input.txt)
        template <typename T>
        void add_positional(std::string name, T& target, std::string desc) {
            positionals_.push_back({
                std::move(name),
                std::move(desc),
                [&target](std::string_view sv) {
                    return parse_value(sv, target);
                },
                false
            });
        }

        // Core Tokenizer and Dispatch Loop
        bool parse(int argc, char* argv[]) {
            if (argc > 0 && argv != nullptr) {
                program_name_ = argv[0];
            }

            size_t next_positional = 0;

            for (int i = 1; i < argc; ++i) {
                std::string_view arg = argv[i];

                // Help handler
                if (arg == "-h" || arg == "--help") {
                    help_requested_ = true;
                    print_help();
                    return false;
                }

                // Posix esq end of operation markers
                if (arg == "--") {
                    for (++i; i < argc; ++i) {
                        if (next_positional < positionals_.size()) {
                            if (!positionals_[next_positional].callback(argv[i])) {
                                error_message_ = "Invalid value for argument: " + positionals_[next_positional].name;
                                return false;
                            }
                            positionals_[next_positional].matched = true;
                            next_positional++;
                        }
                    }
                    break;
                }

                // Flags and Named Options (prefixed with '-')
                if (arg.size() > 1 && arg[0] == '-' && !std::isdigit(static_cast<unsigned char>(arg[1]))) {
                    std::string_view key = arg;
                    std::string_view inline_val;
                    bool has_equals = false;

                    // Split on '=' if present (--intensity=10)
                    if (const auto eq_pos = arg.find('='); eq_pos != std::string_view::npos) {
                        key = arg.substr(0, eq_pos);
                        inline_val = arg.substr(eq_pos + 1);
                        has_equals = true;
                    }

                    Option* opt = find_option(key);
                    if (!opt) {
                        error_message_ = "Unknown argument: " + std::string(key);
                        return false;
                    }

                    if (opt->is_flag) {
                        if (has_equals) {
                            error_message_ = "Flag '" + std::string(key) + "' takes no value.";
                            return false;
                        }
                        if (!opt->callback("")) {
                            error_message_ = "Failed to process flag: " + std::string(key);
                        }
                    } else {
                        if (has_equals) {
                            if (!opt->callback(inline_val)) {
                                error_message_ = "Invalid value '" + std::string(inline_val) + "' for argument " + std::string(key);
                                return false;
                            }
                        } else {
                            if (i + 1 >= argc || argv[i + 1][0] == '-') {
                                error_message_ = "Missing required value for argument: " + std::string(key);
                                return false;
                            }
                            std::string_view next_arg = argv[++i];
                            if (!opt->callback(next_arg)) {
                                error_message_ = "Invalid value '" + std::string(next_arg) + "' for argument " + std::string(key);
                                return false;
                            }
                        }
                    }
                } else {
                    // positional arguments
                    if (next_positional < positionals_.size()) {
                        if (!positionals_[next_positional].callback(arg)) {
                            error_message_ = "Invalid value for positional argument: " + positionals_[next_positional].name;
                            return false;
                        }
                        positionals_[next_positional].matched = true;
                        next_positional++;
                    } else {
                        error_message_ = "Unexpected positional argument: " + std::string(arg);
                        return false;
                    }
                }
            }
            return true;
        }

        void print_help(std::ostream& os = std::cout) const {
            os << "Usage: " << program_name_;
            if (!options_.empty()) os << " [options]";
            for (const auto& pos : positionals_) {
                os << " <" << pos.name << ">";
            }
            os << "\n";

            if (!description_.empty()) {
                os << "\n" << description_ << "\n";
            }

            if (!positionals_.empty()) {
                os << "\nPositional arguments:\n";
                for (const auto& pos : positionals_) {
                    os << "  " << std::left << std::setw(22) << pos.name
                       << pos.description << "\n";
                }
            }

            os << "\nOptions:\n";
            os << "  " << std::left << std::setw(22) << "-h, --help"
               << "Displays this help menu\n";

            for (const auto& opt : options_) {
                std::string label;
                if (!opt.short_name.empty() && !opt.long_name.empty()) {
                    label = opt.short_name + ", " + opt.long_name;
                } else if (!opt.long_name.empty()) {
                    label = "    " + opt.long_name;
                } else {
                    label = opt.short_name;
                }

                if (!opt.is_flag) {
                    label += " <val>";
                }

                os << "  " << std::left << std::setw(22) << label
                   << opt.description << "\n";
            }
        }

        [[nodiscard]] std::string error() const { return error_message_; }
        [[nodiscard]] bool help_requested() const { return help_requested_; }

    private:
        Option* find_option(std::string_view key) {
            for (auto& opt : options_) {
                if ((!opt.short_name.empty() && opt.short_name == key) ||
                    (!opt.long_name.empty() && opt.long_name == key)) {
                    return &opt;
                }
            }
            return nullptr;
        }
    };

    }
