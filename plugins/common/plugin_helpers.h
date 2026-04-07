#pragma once

#include "plotapp/PluginApi.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <vector>

inline char* plotapp_dup_string(const std::string& value) {
    char* out = static_cast<char*>(std::malloc(value.size() + 1));
    if (!out) return nullptr;
    std::memcpy(out, value.c_str(), value.size() + 1);
    return out;
}

inline std::string get_param(const char* params, const std::string& key, const std::string& fallback = "") {
    if (!params || !*params) return fallback;
    std::string text(params);
    const std::string pattern = key + "=";
    std::size_t pos = text.find(pattern);
    if (pos == std::string::npos) return fallback;
    pos += pattern.size();
    std::size_t end = text.find(';', pos);
    return text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
}

inline int get_param_int(const char* params, const std::string& key, int fallback, int minValue) {
    const auto raw = get_param(params, key, "");
    if (raw.empty()) return fallback;
    return std::max(minValue, std::stoi(raw));
}

inline double get_param_double(const char* params, const std::string& key, double fallback) {
    const auto raw = get_param(params, key, "");
    if (raw.empty()) return fallback;
    return std::stod(raw);
}

inline std::vector<double> solve_linear_system(std::vector<std::vector<double>> a, std::vector<double> b) {
    const int n = static_cast<int>(b.size());
    for (int i = 0; i < n; ++i) {
        int pivot = i;
        for (int r = i + 1; r < n; ++r) {
            if (std::fabs(a[r][i]) > std::fabs(a[pivot][i])) pivot = r;
        }
        if (std::fabs(a[pivot][i]) < 1e-12) return {};
        std::swap(a[i], a[pivot]);
        std::swap(b[i], b[pivot]);
        const double diag = a[i][i];
        for (int c = i; c < n; ++c) a[i][c] /= diag;
        b[i] /= diag;
        for (int r = 0; r < n; ++r) {
            if (r == i) continue;
            const double factor = a[r][i];
            for (int c = i; c < n; ++c) a[r][c] -= factor * a[i][c];
            b[r] -= factor * b[i];
        }
    }
    return b;
}

inline double evaluate_polynomial(const std::vector<double>& coeffs, double x) {
    double y = 0.0;
    double power = 1.0;
    for (double c : coeffs) {
        y += c * power;
        power *= x;
    }
    return y;
}

inline int run_polynomial_fit(const PlotAppPluginRequest* request, PlotAppPluginResult* result, int degree, const char* suffix) {
    if (!request || !result || request->source_layer.point_count < static_cast<std::size_t>(degree + 1)) return 1;
    const auto n = request->source_layer.point_count;
    std::vector<std::vector<double>> a(degree + 1, std::vector<double>(degree + 1, 0.0));
    std::vector<double> b(degree + 1, 0.0);
    std::vector<double> powerSums((degree + 1) * 2, 0.0);
    double minX = request->source_layer.points[0].x;
    double maxX = request->source_layer.points[0].x;
    for (std::size_t i = 0; i < n; ++i) {
        const double x = request->source_layer.points[i].x;
        const double y = request->source_layer.points[i].y;
        double power = 1.0;
        for (double& v : powerSums) {
            v += power;
            power *= x;
        }
        power = 1.0;
        for (int row = 0; row <= degree; ++row) {
            b[row] += y * power;
            power *= x;
        }
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
    }
    for (int row = 0; row <= degree; ++row) {
        for (int col = 0; col <= degree; ++col) a[row][col] = powerSums[static_cast<std::size_t>(row + col)];
    }
    auto coeffs = solve_linear_system(a, b);
    if (coeffs.empty()) return 2;

    const int samples = get_param_int(request->params, "samples", 256, 2);
    result->point_count = static_cast<std::size_t>(samples);
    result->points = static_cast<PlotAppPoint*>(std::malloc(sizeof(PlotAppPoint) * result->point_count));
    if (!result->points) return 3;
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(samples - 1);
        const double x = minX + (maxX - minX) * t;
        result->points[i] = PlotAppPoint{x, evaluate_polynomial(coeffs, x)};
    }
    std::ostringstream name;
    name << request->source_layer.layer_name << " / " << suffix;
    result->suggested_layer_name = plotapp_dup_string(name.str());
    result->warning_message = nullptr;
    return 0;
}
