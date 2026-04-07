#include "plotapp/BuildInfo.h"
#include "plotapp/CommandDispatcher.h"
#include "plotapp/ManagedInstall.h"
#include "plotapp/FormulaEvaluator.h"
#include "plotapp/Importer.h"
#include "plotapp/LayerSampler.h"
#include "plotapp/ProjectController.h"
#include "plotapp/ProjectSerializer.h"
#include "plotapp/SvgRenderer.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using namespace plotapp;

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::filesystem::path sourceDir() {
    return std::filesystem::path(PLOTAPP_TEST_SOURCE_DIR);
}

std::filesystem::path buildDir() {
    return std::filesystem::path(PLOTAPP_TEST_BINARY_DIR);
}

std::filesystem::path examplePath(const std::string& name) {
    return sourceDir() / "examples" / name;
}

std::filesystem::path pluginDir() {
    return buildDir() / "plugins";
}

void writeTextFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << content;
}

bool hasPlugin(const PluginManager& manager, const std::string& id) {
    for (const auto& plugin : manager.plugins()) {
        if (plugin.id == id) return true;
    }
    return false;
}

void test_build_info_version_string() {
    std::ifstream in(sourceDir() / "VERSION.txt");
    std::string expected;
    std::getline(in, expected);
    require(!expected.empty(), "VERSION.txt should not be empty");
    require(plotappVersionString() == expected, "Build info version string mismatch");
}

void test_managed_install_manifest_parsing() {
    const auto manifestPath = buildDir() / "test_tmp" / "installation.manifest";
    writeTextFile(manifestPath,
                  "# Managed by PlotApp desktop manager\n"
                  "manifest_version=1\n"
                  "app_name=PlotApp\n"
                  "app_id=plotapp\n"
                  "install_home=/tmp/plotapp-install\n"
                  "system_manager_path=/tmp/plotapp-install/system/desktop_manager.sh\n"
                  "installed_at=2026-04-06T11:22:33Z\n"
                  "installed_version=fix bug v8\n"
                  "source_commit=abcdef1234567890\n"
                  "source_commit_short=abcdef123456\n"
                  "repo_url=https://github.com/example/plotapp.git\n"
                  "branch=main\n"
                  "update_launcher_path=/tmp/bin/plotapp-update\n");

    auto info = loadManagedInstallInfo(manifestPath);
    require(info.has_value(), "Managed install manifest should be parsed");
    require(info->valid, "Managed install info should be marked valid");
    require(info->installHome == "/tmp/plotapp-install", "Managed install home mismatch");
    require(info->systemManagerPath == "/tmp/plotapp-install/system/desktop_manager.sh", "Managed install manager path mismatch");
    require(info->installedVersion == "fix bug v8", "Managed install version mismatch");
    require(info->sourceCommit == "abcdef1234567890", "Managed install commit mismatch");
    require(info->sourceCommitShort == "abcdef123456", "Managed install short commit mismatch");
    require(info->repoUrl == "https://github.com/example/plotapp.git", "Managed install repo mismatch");
    require(info->branch == "main", "Managed install branch mismatch");

    const auto foundManifest = findManagedInstallManifest({buildDir() / "missing.manifest", manifestPath});
    require(foundManifest.has_value() && *foundManifest == manifestPath, "Managed install finder should pick the first existing manifest");
}

void test_managed_install_update_status_parsing() {
    const std::string sampleOutput =
        "Installed version : fix bug v8\n"
        "Installed commit  : abcdef1234567890\n"
        "Update source     : https://github.com/example/plotapp.git\n"
        "Remote branch     : main\n"
        "Remote commit     : fedcba9876543210\n"
        "Status            : update available\n"
        "[plotapp-manager] extra diagnostic line\n";

    const auto parsed = parseManagedInstallUpdateStatus(sampleOutput);
    require(parsed.valid, "Update status output should be parsed");
    require(parsed.installedVersion == "fix bug v8", "Parsed installed version mismatch");
    require(parsed.installedCommit == "abcdef1234567890", "Parsed installed commit mismatch");
    require(parsed.updateSource == "https://github.com/example/plotapp.git", "Parsed update source mismatch");
    require(parsed.remoteBranch == "main", "Parsed remote branch mismatch");
    require(parsed.remoteCommit == "fedcba9876543210", "Parsed remote commit mismatch");
    require(parsed.status == "update available", "Parsed update status mismatch");
}

void test_csv_import() {
    DelimitedTextImporter importer;
    auto table = importer.load(examplePath("sample_points.csv").string());
    require(table.headers.size() == 2, "CSV headers size mismatch");
    auto series = extractNumericSeries(table, 0, 1);
    require(series.points.size() == 5, "CSV point count mismatch");
}

void test_txt_import() {
    DelimitedTextImporter importer;
    auto table = importer.load(examplePath("sample_noise.txt").string());
    auto series = extractNumericSeries(table, 0, 1);
    require(series.points.size() == 6, "TXT point count mismatch");
}

void test_extensionless_import_and_binary_rejection() {
    const auto tmpDir = buildDir() / "test_tmp";
    std::filesystem::create_directories(tmpDir);

    const auto extless = tmpDir / "points_data";
    {
        std::ifstream in(examplePath("sample_points.csv"));
        std::ofstream out(extless);
        out << in.rdbuf();
    }

    DelimitedTextImporter importer;
    require(importer.supports(extless.string()), "Extensionless delimited file should be supported");
    auto table = importer.load(extless.string());
    auto series = extractNumericSeries(table, 0, 1);
    require(series.points.size() == 5, "Extensionless import point count mismatch");

    const auto fakePng = tmpDir / "fake_png";
    {
        std::ofstream out(fakePng, std::ios::binary);
        const unsigned char bytes[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n', 0x00, 0x01, 0x02};
        out.write(reinterpret_cast<const char*>(bytes), static_cast<std::streamsize>(sizeof(bytes)));
    }
    require(!importer.supports(fakePng.string()), "PNG-like binary blob must not be treated as delimited text");
}

void test_xlsx_import() {
    XlsxImporter importer;
    auto table = importer.load(examplePath("sample_book.xlsx").string());
    require(table.sheetName == "Experiment", "XLSX sheet name mismatch");
    auto series = extractNumericSeries(table, 0, 1);
    require(series.points.size() == 6, "XLSX point count mismatch");
}

void test_non_finite_numeric_input_rejected() {
    const auto tmpDir = buildDir() / "test_tmp";
    const auto csvPath = tmpDir / "non_finite_only.csv";
    writeTextFile(csvPath,
                  "x,y\n"
                  "0,nan\n"
                  "1,inf\n");

    DelimitedTextImporter importer;
    auto table = importer.load(csvPath.string());
    bool threw = false;
    try {
        (void)extractNumericSeries(table, 0, 1);
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "Delimited import should reject non-finite numeric rows when nothing finite remains");

    ProjectController controller;
    auto& raw = controller.createManualLayer("raw");
    threw = false;
    try {
        controller.addPoint(raw.id, Point{std::numeric_limits<double>::infinity(), 1.0});
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "Manual point insertion should reject non-finite coordinates");
    require(raw.points.empty(), "Rejected point insertion must not modify the layer");
}

void test_formula_evaluator_and_layer() {
    require(std::abs(FormulaEvaluator::evaluate("sin(x)", 0.0)) < 1e-9, "Formula evaluator failed");
    require(std::abs(FormulaEvaluator::evaluate("1e-3*x", 1000.0) - 1.0) < 1e-9, "Scientific notation parsing failed");
    require(std::abs(FormulaEvaluator::evaluate("2sin(x)", 3.14159265358979323846 / 2.0) - 2.0) < 1e-9,
            "Implicit multiplication parsing failed");

    ProjectController controller;
    auto& layer = controller.createFormulaLayer("quad", "x^2", -2.0, 2.0, 21);
    require(layer.type == LayerType::FormulaSeries, "Formula layer type mismatch");
    require(layer.points.size() == 21, "Formula layer sample size mismatch");
    require(layer.style.connectPoints, "Formula layer should stay continuous");

    const auto beforeInvalid = controller.project().layers().size();
    bool threw = false;
    try {
        (void)controller.createFormulaLayer("broken", "2+", -2.0, 2.0, 21);
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "Invalid formula should be rejected");
    require(controller.project().layers().size() == beforeInvalid, "Invalid formula must not leak a broken layer into the project");

    threw = false;
    try {
        (void)controller.createFormulaLayer("non_finite", "1/0", -2.0, 2.0, 21);
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "Formula with no finite samples should be rejected");
    require(controller.project().layers().size() == beforeInvalid, "Non-finite formula must not leak a broken layer into the project");
}

void test_viewport_sampling() {
    ProjectController controller;
    auto& raw = controller.createManualLayer("raw");
    raw.points = {{0.0, 0.0}, {1.0, 2.0}, {2.0, 1.0}, {3.0, 3.0}};
    const auto rawId = raw.id;

    auto& formula = controller.createFormulaLayer("sin", "sin(x)", -3.0, 3.0, 8);
    const auto sampledFormula = sampleLayerForViewport(controller.project(), formula, -1.0, 1.0, -2.0, 2.0, 400);
    require(sampledFormula.points.size() >= 128, "Formula viewport sampling should densify to viewport resolution");
    require(sampledFormula.points.front().x >= -1.000001, "Formula sampled x min should follow viewport");
    require(sampledFormula.points.back().x <= 1.000001, "Formula sampled x max should follow viewport");

    controller.pluginManager().addSearchDirectory(pluginDir().string());
    controller.pluginManager().discover();
    auto& fit = controller.applyPlugin("linear_fit", rawId, "samples=16");
    const auto sampledFit = sampleLayerForViewport(controller.project(), fit, 10.0, 20.0, -10.0, 10.0, 300);
    require(!sampledFit.points.empty(), "Viewport sampling should return points for continuous derived layers");
    require(sampledFit.points.front().x >= 10.0 - 1e-9, "Derived sampled x min should follow viewport");
    require(sampledFit.points.back().x <= 20.0 + 1e-9, "Derived sampled x max should follow viewport");

    Project invalidProject;
    auto& invalidFormula = invalidProject.createLayer("broken_formula", LayerType::FormulaSeries);
    invalidFormula.formulaExpression = "2+";
    invalidFormula.points = {{-1.0, 0.0}, {1.0, 0.0}};
    const auto sampledInvalid = sampleLayerForViewport(invalidProject, invalidFormula, -1.0, 1.0, -1.0, 1.0, 200);
    require(sampledInvalid.points.size() == 2, "Invalid formula viewport sampling should fall back to stored points");
}

void test_child_visibility_independent_from_parent() {
    Project project;
    auto& parent = project.createLayer("parent", LayerType::RawSeries);
    parent.visible = false;
    const auto parentId = parent.id;
    auto& child = project.createLayer("child", LayerType::DerivedSeries);
    child.parentLayerId = parentId;
    child.visible = true;

    const auto visible = project.visibleLayers();
    require(visible.size() == 1, "Child layer visibility should stay independent from parent visibility");
    require(visible.front()->id == child.id, "Visible child layer should remain visible when parent is hidden");
}

void test_project_roundtrip() {
    Project project;
    project.settings().title = "Roundtrip";
    project.settings().uiTheme = "dark";
    project.settings().uiFontPercent = 115;
    project.settings().hasCustomViewport = true;
    project.settings().viewXMin = -2.0;
    project.settings().viewXMax = 8.0;

    auto& raw = project.createLayer("raw");
    raw.points = {{0, 1}, {1, 3}, {2, 5}};
    raw.legendText = "I_{raw}";
    raw.errorValues = {0.2, 0.3, 0.4};
    raw.style.color = "#112233";
    raw.style.secondaryColor = "#445566";
    raw.style.connectPoints = false;
    raw.pointRoles = {
        static_cast<int>(PointRole::Normal),
        static_cast<int>(PointRole::Minimum),
        static_cast<int>(PointRole::Maximum)
    };
    raw.pointVisibility = {1, 0, 1};
    raw.importedSourcePath = "roundtrip.csv";
    raw.importedSheetName = "Sheet1";
    raw.importedHeaders = {"x", "y", "err"};
    raw.importedRows = {{"0", "1", "0.2"}, {"1", "3", "0.3"}, {"2", "5", "0.4"}};
    raw.importedRowIndices = {0, 1, 2};
    raw.importedXColumn = 0;
    raw.importedYColumn = 1;
    const auto rawId = raw.id;

    Layer formula;
    formula.id = makeLayerId();
    formula.name = "formula";
    formula.type = LayerType::FormulaSeries;
    formula.formulaExpression = "sin(x)";
    formula.formulaXMin = -3.0;
    formula.formulaXMax = 3.0;
    formula.formulaSamples = 120;
    formula.legendText = "\\alpha_1";
    formula.points = FormulaEvaluator::sample(formula.formulaExpression, formula.formulaXMin, formula.formulaXMax, formula.formulaSamples);
    project.layers().push_back(formula);

    Layer derived;
    derived.id = makeLayerId();
    derived.name = "derived";
    derived.type = LayerType::DerivedSeries;
    derived.points = {{0, 1}, {1, 2}};
    derived.sourceLayerId = rawId;
    derived.generatorPluginId = "linear_fit";
    derived.generatorParams = "samples=16";
    project.layers().push_back(derived);

    const auto path = buildDir() / "tests_roundtrip.plotapp";
    ProjectSerializer::save(project, path.string());
    auto loaded = ProjectSerializer::load(path.string());
    require(loaded.layers().size() == 3, "Roundtrip layer count mismatch");
    require(loaded.settings().uiTheme == "dark", "Roundtrip theme mismatch");
    require(loaded.layers()[0].errorValues.size() == 3, "Roundtrip error values mismatch");
    require(loaded.layers()[0].style.secondaryColor == "#445566", "Roundtrip secondary color mismatch");
    require(!loaded.layers()[0].style.connectPoints, "Roundtrip connectPoints mismatch");
    require(loaded.layers()[0].pointVisibility.size() == 3 && loaded.layers()[0].pointVisibility[1] == 0, "Roundtrip point visibility mismatch");
    require(loaded.layers()[0].importedHeaders.size() == 3, "Roundtrip imported headers mismatch");
    require(loaded.layers()[0].importedRowIndices.size() == 3, "Roundtrip imported row indices mismatch");
    require(loaded.layers()[1].type == LayerType::FormulaSeries, "Roundtrip formula type mismatch");
}


void test_project_save_avoids_predictable_temp_name() {
    Project project;
    auto& raw = project.createLayer("raw");
    raw.points = {{0.0, 1.0}, {1.0, 2.0}};

    const auto path = buildDir() / "secure_save.plotapp";
    const auto legacyTmp = std::filesystem::path(path.string() + ".tmp");
    writeTextFile(legacyTmp, "legacy sentinel\n");

    ProjectSerializer::save(project, path.string());
    require(std::filesystem::exists(path), "Secure save did not create the project file");

    std::ifstream in(legacyTmp);
    std::string legacyContent;
    std::getline(in, legacyContent);
    require(legacyContent == "legacy sentinel", "Secure save should not reuse the predictable .tmp sidecar");

    auto loaded = ProjectSerializer::load(path.string());
    require(loaded.layers().size() == 1, "Secure save roundtrip failed");
}

void test_project_resource_limits() {
    constexpr std::uintmax_t kProjectTooLargeOffset = 64u * 1024u * 1024u;
    constexpr std::size_t kTooManyPoints = 200'001u;

    const auto oversizedProject = buildDir() / "oversized.plotapp";
    {
        std::ofstream out(oversizedProject, std::ios::binary);
        out << "PLOTAPP_PROJECT=5\n";
        out.seekp(static_cast<std::streamoff>(kProjectTooLargeOffset));
        out.put('\n');
    }

    bool threw = false;
    try {
        (void)ProjectSerializer::load(oversizedProject.string());
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "Oversized project file should be rejected before parsing");

    const auto tooManyPointsProject = buildDir() / "too_many_points.plotapp";
    {
        std::ofstream out(tooManyPointsProject);
        out << "PLOTAPP_PROJECT=5\n";
        out << "LAYER_BEGIN\n";
        out << "NAME=too_many\n";
        for (std::size_t i = 0; i < kTooManyPoints; ++i) {
            out << "POINT=" << i << ',' << i << '\n';
        }
        out << "LAYER_END\n";
    }

    threw = false;
    try {
        (void)ProjectSerializer::load(tooManyPointsProject.string());
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "Project with too many points should be rejected");

    const auto nonRegularPath = buildDir() / "project_dir_instead_of_file";
    std::filesystem::create_directories(nonRegularPath);
    threw = false;
    try {
        (void)ProjectSerializer::load(nonRegularPath.string());
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "Project loader should reject non-regular file paths");
}

void test_invalid_project_rejected() {
    const auto badProject = buildDir() / "not_a_project.txt";
    {
        std::ofstream out(badProject);
        out << "TITLE=This is not a PlotApp project\n";
    }

    bool threw = false;
    try {
        (void)ProjectSerializer::load(badProject.string());
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "Headerless project file should be rejected");
}

void test_project_numeric_sanitization_and_rejection() {
    const auto badNumericProject = buildDir() / "bad_numeric.plotapp";
    {
        std::ofstream out(badNumericProject);
        out << "PLOTAPP_PROJECT=5\n";
        out << "LAYER_BEGIN\n";
        out << "NAME=bad\n";
        out << "POINT=nan,1\n";
        out << "LAYER_END\n";
    }

    bool threw = false;
    try {
        (void)ProjectSerializer::load(badNumericProject.string());
    } catch (const std::exception&) {
        threw = true;
    }
    require(threw, "Project with non-finite numeric values should be rejected");

    const auto degenerateViewportProject = buildDir() / "degenerate_viewport.plotapp";
    {
        std::ofstream out(degenerateViewportProject);
        out << "PLOTAPP_PROJECT=5\n";
        out << "HAS_CUSTOM_VIEWPORT=1\n";
        out << "VIEW_XMIN=1\n";
        out << "VIEW_XMAX=1\n";
        out << "VIEW_YMIN=2\n";
        out << "VIEW_YMAX=2\n";
        out << "LAYER_BEGIN\n";
        out << "NAME=raw\n";
        out << "POINT=0,0\n";
        out << "POINT=1,1\n";
        out << "LAYER_END\n";
    }

    auto loaded = ProjectSerializer::load(degenerateViewportProject.string());
    require(loaded.settings().hasCustomViewport, "Custom viewport flag should be preserved");
    require(loaded.settings().viewXMin < loaded.settings().viewXMax, "Degenerate custom X viewport should be widened");
    require(loaded.settings().viewYMin < loaded.settings().viewYMax, "Degenerate custom Y viewport should be widened");
}

void test_plugins_and_recompute() {
    const auto tmpDir = buildDir() / "test_tmp";
    const auto csvPath = tmpDir / "plugin_wave.csv";
    writeTextFile(csvPath,
                  "x,y,err\n"
                  "0,0,0.2\n"
                  "1,2,0.4\n"
                  "2,0,0.6\n"
                  "3,2,0.8\n"
                  "4,0,1.0\n");

    ProjectController controller;
    controller.pluginManager().addSearchDirectory(pluginDir().string());
    controller.pluginManager().discover();
    require(hasPlugin(controller.pluginManager(), "linear_fit"), "linear_fit plugin missing");
    require(hasPlugin(controller.pluginManager(), "newton_polynomial"), "newton_polynomial plugin missing");
    require(hasPlugin(controller.pluginManager(), "smooth_curve"), "smooth_curve plugin missing");
    require(hasPlugin(controller.pluginManager(), "error_bars"), "error_bars plugin missing");
    require(hasPlugin(controller.pluginManager(), "local_extrema"), "local_extrema plugin missing");

    auto& raw = controller.importLayer(csvPath.string(), 0, 1, "wave");
    const auto rawId = raw.id;

    auto& derived = controller.applyPlugin("linear_fit", rawId, "samples=8");
    require(derived.points.size() == 8, "Derived layer sample count mismatch");

    auto& poly = controller.applyPlugin("newton_polynomial", rawId, "degree=3;samples=40");
    require(poly.points.size() == 40, "Configurable Newton plugin sample count mismatch");
    require(poly.generatorPluginId == "newton_polynomial", "Configurable Newton plugin id mismatch");

    auto& smooth = controller.applyPlugin("smooth_curve", rawId, "samples=50");
    require(smooth.points.size() == 50, "Smooth curve plugin sample count mismatch");
    require(smooth.style.connectPoints, "Smooth curve plugin should stay continuous");

    auto& extrema = controller.applyPlugin("local_extrema", rawId, "mode=both;window=1;tolerance=0.0");
    require(extrema.style.showMarkers, "Extrema layer should show markers");
    require(!extrema.style.connectPoints, "Extrema layer must not connect points");
    require(extrema.pointRoles.size() == extrema.points.size(), "Extrema point roles must match point count");
    require(extrema.points.size() >= 3, "Expected local extrema points were not found");
    extrema.style.color = "#00ff00";
    extrema.style.secondaryColor = "#ff00ff";
    if (!extrema.pointVisibility.empty()) extrema.pointVisibility[0] = 0;
    const auto extremaId = extrema.id;

    auto& err = controller.applyPlugin("error_bars", rawId, "column=err");
    require(err.errorValues.size() == err.points.size(), "Error bars should carry per-point errors");
    require(!err.style.connectPoints, "Error bars layer must not connect points");
    require(!err.style.showMarkers, "Error bars layer should not show markers by default");
    require(std::abs(err.errorValues[2] - 0.6) < 1e-9, "Error bars should use imported numeric column");

    controller.pluginManager().discover();
    require(hasPlugin(controller.pluginManager(), "smooth_curve"), "Plugins disappeared after rediscovery");

    const auto savedPath = buildDir() / "plugin_project.plotapp";
    controller.saveProject(savedPath.string());
    controller.openProject(savedPath.string());
    auto warnings = controller.recomputeDerivedLayers();
    (void)warnings;

    auto* reopenedExtrema = controller.project().findLayer(extremaId);
    require(reopenedExtrema != nullptr, "Reopened extrema layer missing");
    require(!reopenedExtrema->style.connectPoints, "Recomputed extrema layer must stay disconnected");
    require(reopenedExtrema->style.color == "#00ff00", "User primary color should survive recompute");
    require(reopenedExtrema->style.secondaryColor == "#ff00ff", "User secondary color should survive recompute");
    require(!reopenedExtrema->pointVisibility.empty() && reopenedExtrema->pointVisibility[0] == 0, "User hidden extrema point should survive recompute");
}

void test_svg_export_security_and_formula_render() {
    Project project;
    project.settings().title = "<Unsafe & Title>";
    project.settings().xLabel = "X & <x>";
    project.settings().yLabel = "Y";
    project.settings().uiTheme = "dark";
    project.settings().hasCustomViewport = true;
    project.settings().viewXMin = -3.0;
    project.settings().viewXMax = 3.0;
    project.settings().viewYMin = -4.0;
    project.settings().viewYMax = 4.0;

    auto& formula = project.createLayer("formula", LayerType::FormulaSeries);
    formula.formulaExpression = "sin(x)";
    formula.formulaXMin = -1.0;
    formula.formulaXMax = 1.0;
    formula.formulaSamples = 4;
    formula.legendText = "<legend>";
    formula.style.color = "\"><script>alert(1)</script>";
    formula.style.showMarkers = false;
    formula.style.connectPoints = true;
    formula.points.clear();

    auto svg = SvgRenderer::renderToString(project, 800, 600);
    require(svg.find("&lt;Unsafe &amp; Title&gt;") != std::string::npos, "SVG title should be escaped");
    require(svg.find("<script>") == std::string::npos, "SVG must not contain injected script tags");
    require(svg.find("#1f77b4") != std::string::npos, "Unsafe SVG color should fall back to default");
    require(svg.find("<path") != std::string::npos, "Formula layer should render as an SVG path");
    require(svg.find("fill=\"none\"") != std::string::npos, "Continuous SVG path should not be filled");
    require(svg.find("fill=\"#e8eaed\"") != std::string::npos, "Dark theme text should export with light foreground");
}

void test_command_dispatcher() {
    ProjectController controller;
    controller.pluginManager().addSearchDirectory(pluginDir().string());
    controller.pluginManager().discover();
    CommandDispatcher dispatcher(controller);
    auto helpText = dispatcher.execute("help");
    require(helpText.find("formula") != std::string::npos, "Help text missing formula command");
    dispatcher.execute(std::string("import ") + examplePath("sample_points.csv").string() + " 0 1 test");
    dispatcher.execute("formula x^2 -2 2 parabola");
    auto list = dispatcher.execute("list-layers");
    require(list.find("test") != std::string::npos, "Dispatcher list output missing layer");
    require(list.find("parabola") != std::string::npos, "Dispatcher list output missing formula layer");
    auto pwd = dispatcher.execute("pwd");
    require(!pwd.empty(), "pwd returned empty output");

    const auto shellResult = dispatcher.execute("!echo should_not_run");
    require(shellResult.find("disabled by default") != std::string::npos, "Shell passthrough should be disabled by default");
}

} // namespace

int main() {
    try {
        test_build_info_version_string();
        test_managed_install_manifest_parsing();
        test_managed_install_update_status_parsing();
        test_csv_import();
        test_txt_import();
        test_extensionless_import_and_binary_rejection();
        test_xlsx_import();
        test_non_finite_numeric_input_rejected();
        test_formula_evaluator_and_layer();
        test_viewport_sampling();
        test_child_visibility_independent_from_parent();
        test_project_roundtrip();
        test_project_save_avoids_predictable_temp_name();
        test_project_resource_limits();
        test_invalid_project_rejected();
        test_project_numeric_sanitization_and_rejection();
        test_plugins_and_recompute();
        test_svg_export_security_and_formula_render();
        test_command_dispatcher();
        std::cout << "All tests passed.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "TEST FAILURE: " << ex.what() << '\n';
        return 1;
    }
}
