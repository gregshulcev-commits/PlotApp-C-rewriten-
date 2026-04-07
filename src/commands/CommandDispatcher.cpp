#include "plotapp/CommandDispatcher.h"
#include "plotapp/TextUtil.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace plotapp {
namespace {

std::string joinFrom(const std::vector<std::string>& tokens, std::size_t startIndex) {
    std::ostringstream out;
    for (std::size_t i = startIndex; i < tokens.size(); ++i) {
        if (i > startIndex) out << ' ';
        out << tokens[i];
    }
    return out.str();
}

std::string shellQuote(const std::string& value) {
    std::string out = "'";
    for (char c : value) {
        if (c == '\'') out += "'\\''";
        else out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

bool isUnsignedInteger(const std::string& value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isdigit(c) != 0; });
}

bool isInteger(const std::string& value) {
    if (value.empty()) return false;
    std::size_t index = 0;
    if (value[0] == '+' || value[0] == '-') index = 1;
    return index < value.size() && std::all_of(value.begin() + static_cast<std::ptrdiff_t>(index), value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

bool shellCommandsEnabled() {
    const char* raw = std::getenv("PLOTAPP_ENABLE_SHELL");
    if (!raw) return false;
    const std::string value = text::toLower(text::trim(raw));
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

} // namespace

CommandDispatcher::CommandDispatcher(ProjectController& controller)
    : controller_(controller), currentDirectory_(std::filesystem::current_path()) {}

std::vector<std::string> CommandDispatcher::builtinCommands() {
    return {
        "help", "pwd", "cd", "ls", "new-project", "title", "labels", "import", "formula",
        "list-layers", "add-point", "apply-plugin", "toggle-layer", "save", "open", "export-svg", "plugins"
    };
}

std::vector<std::string> CommandDispatcher::completions(const std::string& prefix) const {
    std::vector<std::string> out;
    for (const auto& command : builtinCommands()) {
        if (command.rfind(prefix, 0) == 0) out.push_back(command);
    }
    return out;
}

std::string CommandDispatcher::bundledHelpText() {
    return
        "PlotApp command help\n"
        "====================\n"
        "Built-in commands:\n"
        "  help                              Show this help text\n"
        "  pwd                               Show current working directory\n"
        "  cd <dir>                          Change working directory for this console\n"
        "  ls [dir]                          List files\n"
        "  new-project                       Reset current project\n"
        "  title <text>                      Set project title\n"
        "  labels <x_label> <y_label>        Set axis labels\n"
        "  import <path> <x> <y> [name] [e]  Import x/y columns and optional error column\n"
        "  formula <expr> <xmin> <xmax> [samples] [name]\n"
        "  list-layers                       List layers\n"
        "  add-point <layer> <x> <y>         Add point to layer\n"
        "  apply-plugin <plugin> <layer> [params]\n"
        "  toggle-layer <layer>              Toggle layer visibility\n"
        "  save <path>                       Save project (*.plotapp)\n"
        "  open <path>                       Open project\n"
        "  export-svg <path>                 Export visible layers to SVG\n"
        "  plugins                           List discovered plugins\n"
        "  !<bash command>                   Run a bash command in the console cwd (requires PLOTAPP_ENABLE_SHELL=1)\n"
        "\n"
        "Examples:\n"
        "  import ./examples/sample_points.csv 0 1 raw\n"
        "  import ./measurements.csv 0 1 raw 2\n"
        "  formula \"sin(x)+0.2*x^2\" -10 10 512 curve\n"
        "  formula x^2 -2 2 parabola\n"
        "  apply-plugin linear_fit <layer_id> samples=256\n";
}

std::string CommandDispatcher::help() const {
    const std::vector<std::filesystem::path> candidates = {
        currentDirectory_ / "docs/COMMAND_HELP.txt",
        std::filesystem::current_path() / "docs/COMMAND_HELP.txt",
        std::filesystem::path("docs/COMMAND_HELP.txt")
    };
    for (const auto& candidate : candidates) {
        std::ifstream in(candidate);
        if (!in) continue;
        std::ostringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }
    return bundledHelpText();
}

std::filesystem::path CommandDispatcher::resolvePath(const std::string& raw) const {
    const auto trimmed = text::trim(raw);
    if (trimmed.empty()) return currentDirectory_;
    std::filesystem::path path(trimmed);
    return path.is_absolute() ? path : (currentDirectory_ / path);
}

std::string CommandDispatcher::executeShell(const std::string& commandLine) const {
    if (!shellCommandsEnabled()) {
        return "Error: shell commands are disabled by default. Set PLOTAPP_ENABLE_SHELL=1 to enable them.";
    }
    if (commandLine.size() <= 1) return "Error: empty shell command";
    const std::string script = "cd " + shellQuote(currentDirectory_.string()) + " && " + commandLine.substr(1) + " 2>&1";
    std::array<char, 512> buffer {};
    std::string output;
    FILE* pipe = popen((std::string("bash -lc ") + shellQuote(script)).c_str(), "r");
    if (!pipe) return "Error: failed to start shell command";
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
        output += buffer.data();
    }
    const int code = pclose(pipe);
    if (output.empty()) output = code == 0 ? "Shell command finished." : "Shell command failed.";
    return output;
}

std::string CommandDispatcher::execute(const std::string& commandLine) {
    const auto trimmed = text::trim(commandLine);
    if (trimmed.empty()) return {};
    if (!trimmed.empty() && trimmed.front() == '!') return executeShell(trimmed);

    auto tokens = text::shellSplit(trimmed);
    if (tokens.empty()) return {};

    std::ostringstream out;
    const auto& cmd = tokens[0];
    try {
        if (cmd == "help") {
            return help();
        }
        if (cmd == "pwd") {
            return currentDirectory_.string();
        }
        if (cmd == "cd") {
            auto target = tokens.size() >= 2 ? resolvePath(tokens[1]) : std::filesystem::path(std::getenv("HOME") ? std::getenv("HOME") : ".");
            target = std::filesystem::weakly_canonical(target);
            if (!std::filesystem::exists(target) || !std::filesystem::is_directory(target)) {
                throw std::runtime_error("Directory not found: " + target.string());
            }
            currentDirectory_ = target;
            return currentDirectory_.string();
        }
        if (cmd == "ls") {
            const auto target = tokens.size() >= 2 ? resolvePath(tokens[1]) : currentDirectory_;
            if (!std::filesystem::exists(target)) throw std::runtime_error("Path not found: " + target.string());
            std::vector<std::filesystem::directory_entry> entries;
            for (const auto& entry : std::filesystem::directory_iterator(target)) entries.push_back(entry);
            std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                return a.path().filename().string() < b.path().filename().string();
            });
            for (const auto& entry : entries) {
                out << (entry.is_directory() ? "[D] " : "[F] ") << entry.path().filename().string() << '\n';
            }
            return out.str();
        }
        if (cmd == "plugins") {
            for (const auto& plugin : controller_.pluginManager().plugins()) {
                out << plugin.id << "\t" << plugin.name << "\t" << plugin.defaultParams << '\n';
            }
            return out.str();
        }
        if (cmd == "new-project") {
            controller_.reset();
            return "Project reset.";
        }
        if (cmd == "title" && tokens.size() >= 2) {
            controller_.project().settings().title = trimmed.substr(trimmed.find(' ') + 1);
            return "Title updated.";
        }
        if (cmd == "labels" && tokens.size() >= 3) {
            controller_.project().settings().xLabel = tokens[1];
            controller_.project().settings().yLabel = tokens[2];
            return "Axis labels updated.";
        }
        if (cmd == "import" && tokens.size() >= 4) {
            std::optional<std::size_t> errorColumn = std::nullopt;
            std::string layerName;
            if (tokens.size() >= 5) {
                if (isUnsignedInteger(tokens[4]) && tokens.size() == 5) errorColumn = static_cast<std::size_t>(std::stoul(tokens[4]));
                else layerName = tokens[4];
            }
            if (tokens.size() >= 6) errorColumn = static_cast<std::size_t>(std::stoul(tokens[5]));
            auto& layer = controller_.importLayer(resolvePath(tokens[1]).string(), static_cast<std::size_t>(std::stoul(tokens[2])), static_cast<std::size_t>(std::stoul(tokens[3])), layerName, errorColumn);
            out << "Imported layer " << layer.name << " (id=" << layer.id << ") with " << layer.points.size() << " points.";
            if (!layer.errorValues.empty()) out << " Error values: " << layer.errorValues.size() << '.';
            return out.str();
        }
        if (cmd == "formula" && tokens.size() >= 4) {
            int samples = 512;
            std::size_t nameIndex = 5;
            if (tokens.size() >= 5 && isInteger(tokens[4])) {
                samples = std::stoi(tokens[4]);
            } else if (tokens.size() >= 5) {
                nameIndex = 4;
            }
            const std::string name = tokens.size() >= nameIndex ? joinFrom(tokens, nameIndex) : "Formula";
            auto& layer = controller_.createFormulaLayer(name, tokens[1], std::stod(tokens[2]), std::stod(tokens[3]), samples);
            out << "Formula layer created: " << layer.name << " (id=" << layer.id << ") with " << layer.points.size() << " samples.";
            return out.str();
        }
        if (cmd == "list-layers") {
            for (const auto& layer : controller_.project().layers()) {
                out << layer.id << "\t" << layer.name << "\t" << layerTypeToString(layer.type) << "\t" << (layer.visible ? "visible" : "hidden") << "\t" << layer.points.size() << " pts";
                if (!layer.errorValues.empty()) out << "\t" << layer.errorValues.size() << " err";
                out << '\n';
            }
            return out.str();
        }
        if (cmd == "add-point" && tokens.size() >= 4) {
            controller_.addPoint(tokens[1], Point{std::stod(tokens[2]), std::stod(tokens[3])});
            return "Point added.";
        }
        if (cmd == "apply-plugin" && tokens.size() >= 3) {
            const std::string params = tokens.size() >= 4 ? joinFrom(tokens, 3) : "";
            auto& layer = controller_.applyPlugin(tokens[1], tokens[2], params);
            out << "Derived layer created: " << layer.name << " (id=" << layer.id << ")";
            return out.str();
        }
        if (cmd == "toggle-layer" && tokens.size() >= 2) {
            auto* layer = controller_.project().findLayer(tokens[1]);
            if (!layer) throw std::runtime_error("Layer not found");
            layer->visible = !layer->visible;
            return layer->visible ? "Layer shown." : "Layer hidden.";
        }
        if (cmd == "save" && tokens.size() >= 2) {
            auto path = resolvePath(tokens[1]);
            if (path.extension().empty()) path += ".plotapp";
            controller_.saveProject(path.string());
            return "Project saved to " + path.string();
        }
        if (cmd == "open" && tokens.size() >= 2) {
            controller_.openProject(resolvePath(tokens[1]).string());
            auto warnings = controller_.recomputeDerivedLayers();
            out << "Project opened.";
            for (const auto& warning : warnings) out << "\nWarning: " << warning;
            return out.str();
        }
        if (cmd == "export-svg" && tokens.size() >= 2) {
            auto path = resolvePath(tokens[1]);
            if (path.extension().empty()) path += ".svg";
            controller_.exportSvg(path.string());
            return "SVG exported to " + path.string();
        }
        return "Unknown or invalid command. Try 'help'.";
    } catch (const std::exception& ex) {
        return std::string("Error: ") + ex.what();
    }
}

} // namespace plotapp
