#pragma once

#include "plotapp/Project.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace plotapp::math {

struct LinearModel {
    double slope {0.0};
    double intercept {0.0};
    bool valid {false};
};

inline std::vector<Point> sortAndDedupePointsByX(const std::vector<Point>& points) {
    std::vector<Point> sorted;
    sorted.reserve(points.size());
    for (const auto& point : points) {
        if (std::isfinite(point.x) && std::isfinite(point.y)) sorted.push_back(point);
    }
    std::sort(sorted.begin(), sorted.end(), [](const Point& a, const Point& b) {
        if (a.x == b.x) return a.y < b.y;
        return a.x < b.x;
    });
    if (sorted.empty()) return sorted;

    std::vector<Point> unique;
    unique.reserve(sorted.size());
    Point current = sorted.front();
    int count = 1;
    for (std::size_t i = 1; i < sorted.size(); ++i) {
        if (std::fabs(sorted[i].x - current.x) <= 1e-12) {
            current.y += sorted[i].y;
            ++count;
        } else {
            current.y /= static_cast<double>(count);
            unique.push_back(current);
            current = sorted[i];
            count = 1;
        }
    }
    current.y /= static_cast<double>(count);
    unique.push_back(current);
    return unique;
}

inline std::vector<double> solveLinearSystem(std::vector<std::vector<double>> a, std::vector<double> b) {
    const int n = static_cast<int>(b.size());
    for (int i = 0; i < n; ++i) {
        int pivot = i;
        for (int row = i + 1; row < n; ++row) {
            if (std::fabs(a[row][i]) > std::fabs(a[pivot][i])) pivot = row;
        }
        if (std::fabs(a[pivot][i]) < 1e-12) return {};
        std::swap(a[i], a[pivot]);
        std::swap(b[i], b[pivot]);
        const double diag = a[i][i];
        for (int col = i; col < n; ++col) a[i][col] /= diag;
        b[i] /= diag;
        for (int row = 0; row < n; ++row) {
            if (row == i) continue;
            const double factor = a[row][i];
            for (int col = i; col < n; ++col) a[row][col] -= factor * a[i][col];
            b[row] -= factor * b[i];
        }
    }
    return b;
}

inline LinearModel fitLinearModel(const std::vector<Point>& points) {
    LinearModel model;
    if (points.size() < 2) return model;
    double sumX = 0.0;
    double sumY = 0.0;
    double sumXX = 0.0;
    double sumXY = 0.0;
    std::size_t count = 0;
    for (const auto& point : points) {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) continue;
        sumX += point.x;
        sumY += point.y;
        sumXX += point.x * point.x;
        sumXY += point.x * point.y;
        ++count;
    }
    if (count < 2) return model;
    const double denom = static_cast<double>(count) * sumXX - sumX * sumX;
    if (std::fabs(denom) < 1e-12) return model;
    model.slope = (static_cast<double>(count) * sumXY - sumX * sumY) / denom;
    model.intercept = (sumY - model.slope * sumX) / static_cast<double>(count);
    model.valid = true;
    return model;
}

inline std::vector<Point> sampleLinearModel(const LinearModel& model, double xMin, double xMax, int samples) {
    std::vector<Point> out;
    if (!model.valid) return out;
    if (xMin > xMax) std::swap(xMin, xMax);
    samples = std::max(samples, 2);
    out.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(samples - 1);
        const double x = xMin + (xMax - xMin) * t;
        out.push_back(Point{x, model.slope * x + model.intercept});
    }
    return out;
}

inline bool fitPolynomialRegression(const std::vector<Point>& points, int degree, std::vector<double>& coeffs) {
    coeffs.clear();
    if (degree < 1) return false;
    std::vector<Point> filtered;
    filtered.reserve(points.size());
    for (const auto& point : points) {
        if (std::isfinite(point.x) && std::isfinite(point.y)) filtered.push_back(point);
    }
    if (filtered.size() < static_cast<std::size_t>(degree + 1)) return false;

    std::vector<std::vector<double>> a(static_cast<std::size_t>(degree + 1), std::vector<double>(static_cast<std::size_t>(degree + 1), 0.0));
    std::vector<double> b(static_cast<std::size_t>(degree + 1), 0.0);
    std::vector<double> powerSums(static_cast<std::size_t>((degree + 1) * 2), 0.0);

    for (const auto& point : filtered) {
        double power = 1.0;
        for (double& value : powerSums) {
            value += power;
            power *= point.x;
        }
        power = 1.0;
        for (int row = 0; row <= degree; ++row) {
            b[static_cast<std::size_t>(row)] += point.y * power;
            power *= point.x;
        }
    }

    for (int row = 0; row <= degree; ++row) {
        for (int col = 0; col <= degree; ++col) {
            a[static_cast<std::size_t>(row)][static_cast<std::size_t>(col)] = powerSums[static_cast<std::size_t>(row + col)];
        }
    }

    coeffs = solveLinearSystem(std::move(a), std::move(b));
    return !coeffs.empty();
}

inline double evaluatePolynomial(const std::vector<double>& coeffs, double x) {
    double y = 0.0;
    double power = 1.0;
    for (double coeff : coeffs) {
        y += coeff * power;
        power *= x;
    }
    return y;
}

inline std::vector<Point> samplePolynomial(const std::vector<double>& coeffs, double xMin, double xMax, int samples) {
    std::vector<Point> out;
    if (coeffs.empty()) return out;
    if (xMin > xMax) std::swap(xMin, xMax);
    samples = std::max(samples, 2);
    out.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(samples - 1);
        const double x = xMin + (xMax - xMin) * t;
        out.push_back(Point{x, evaluatePolynomial(coeffs, x)});
    }
    return out;
}

class NaturalCubicSpline {
public:
    bool build(const std::vector<Point>& rawPoints) {
        nodes_ = sortAndDedupePointsByX(rawPoints);
        secondDerivatives_.clear();
        if (nodes_.size() < 2) return false;
        if (nodes_.size() == 2) {
            secondDerivatives_.assign(2, 0.0);
            return true;
        }

        const std::size_t n = nodes_.size();
        std::vector<double> u(n - 1, 0.0);
        secondDerivatives_.assign(n, 0.0);

        for (std::size_t i = 1; i + 1 < n; ++i) {
            const double sig = (nodes_[i].x - nodes_[i - 1].x) / (nodes_[i + 1].x - nodes_[i - 1].x);
            const double p = sig * secondDerivatives_[i - 1] + 2.0;
            secondDerivatives_[i] = (sig - 1.0) / p;
            const double ddydx = (nodes_[i + 1].y - nodes_[i].y) / (nodes_[i + 1].x - nodes_[i].x)
                - (nodes_[i].y - nodes_[i - 1].y) / (nodes_[i].x - nodes_[i - 1].x);
            u[i] = (6.0 * ddydx / (nodes_[i + 1].x - nodes_[i - 1].x) - sig * u[i - 1]) / p;
        }
        for (std::size_t k = n - 2; k > 0; --k) {
            secondDerivatives_[k] = secondDerivatives_[k] * secondDerivatives_[k + 1] + u[k];
        }
        return true;
    }

    bool valid() const {
        return nodes_.size() >= 2 && secondDerivatives_.size() == nodes_.size();
    }

    double minX() const { return nodes_.empty() ? 0.0 : nodes_.front().x; }
    double maxX() const { return nodes_.empty() ? 1.0 : nodes_.back().x; }

    double evaluate(double x) const {
        if (!valid()) throw std::runtime_error("Spline is not initialized");
        if (nodes_.size() == 2) {
            const double x0 = nodes_[0].x;
            const double x1 = nodes_[1].x;
            const double t = (x - x0) / (x1 - x0);
            return nodes_[0].y + (nodes_[1].y - nodes_[0].y) * t;
        }
        std::size_t low = 0;
        std::size_t high = nodes_.size() - 1;
        if (x <= nodes_.front().x) {
            low = 0;
            high = 1;
        } else if (x >= nodes_.back().x) {
            low = nodes_.size() - 2;
            high = nodes_.size() - 1;
        } else {
            while (high - low > 1) {
                const std::size_t mid = (low + high) / 2;
                if (nodes_[mid].x > x) high = mid;
                else low = mid;
            }
        }
        const double h = nodes_[high].x - nodes_[low].x;
        if (std::fabs(h) < 1e-12) return nodes_[low].y;
        const double a = (nodes_[high].x - x) / h;
        const double b = (x - nodes_[low].x) / h;
        return a * nodes_[low].y + b * nodes_[high].y
            + ((a * a * a - a) * secondDerivatives_[low] + (b * b * b - b) * secondDerivatives_[high]) * (h * h) / 6.0;
    }

private:
    std::vector<Point> nodes_;
    std::vector<double> secondDerivatives_;
};

inline std::vector<Point> sampleSpline(const std::vector<Point>& points, double xMin, double xMax, int samples, bool clampToDataDomain = true) {
    NaturalCubicSpline spline;
    if (!spline.build(points)) return {};
    if (xMin > xMax) std::swap(xMin, xMax);
    if (clampToDataDomain) {
        xMin = std::max(xMin, spline.minX());
        xMax = std::min(xMax, spline.maxX());
    }
    if (xMin > xMax) return {};
    samples = std::max(samples, 2);
    std::vector<Point> out;
    out.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(samples - 1);
        const double x = xMin + (xMax - xMin) * t;
        const double y = spline.evaluate(x);
        if (std::isfinite(y)) out.push_back(Point{x, y});
    }
    return out;
}

inline std::pair<double, double> finiteXRange(const std::vector<Point>& points) {
    double minX = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    for (const auto& point : points) {
        if (!std::isfinite(point.x)) continue;
        minX = std::min(minX, point.x);
        maxX = std::max(maxX, point.x);
    }
    if (!std::isfinite(minX) || !std::isfinite(maxX)) return {0.0, 1.0};
    if (minX == maxX) return {minX - 1.0, maxX + 1.0};
    return {minX, maxX};
}

} // namespace plotapp::math
