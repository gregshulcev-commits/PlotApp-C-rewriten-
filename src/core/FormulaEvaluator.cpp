#include "plotapp/FormulaEvaluator.h"
#include "plotapp/TextUtil.h"

#include <cmath>
#include <cctype>
#include <stdexcept>
#include <string>

namespace plotapp {
namespace {

class Parser {
public:
    Parser(std::string expr, double xValue) : expr_(std::move(expr)), x_(xValue) {}

    double parse() {
        pos_ = 0;
        const double value = parseExpression();
        skipSpaces();
        if (pos_ != expr_.size()) {
            throw std::runtime_error("Unexpected token in formula near: " + expr_.substr(pos_));
        }
        return value;
    }

private:
    double parseExpression() {
        double value = parseTerm();
        while (true) {
            skipSpaces();
            if (match('+')) value += parseTerm();
            else if (match('-')) value -= parseTerm();
            else break;
        }
        return value;
    }

    double parseTerm() {
        double value = parseUnary();
        while (true) {
            skipSpaces();
            if (match('*')) value *= parseUnary();
            else if (match('/')) value /= parseUnary();
            else if (startsImplicitMultiplication()) value *= parseUnary();
            else break;
        }
        return value;
    }

    double parsePower() {
        double base = parsePrimary();
        skipSpaces();
        if (match('^')) {
            const double exponent = parseUnary();
            return std::pow(base, exponent);
        }
        return base;
    }

    double parseUnary() {
        skipSpaces();
        if (match('+')) return parseUnary();
        if (match('-')) return -parseUnary();
        return parsePower();
    }

    double parsePrimary() {
        skipSpaces();
        if (match('(')) {
            const double value = parseExpression();
            require(')');
            return value;
        }
        if (std::isalpha(static_cast<unsigned char>(peek())) || peek() == '\\') {
            const std::string ident = parseIdentifier();
            if (ident == "x") return x_;
            if (ident == "pi") return 3.14159265358979323846;
            if (ident == "e") return std::exp(1.0);
            skipSpaces();
            if (match('(')) {
                const double arg = parseExpression();
                require(')');
                if (ident == "sin") return std::sin(arg);
                if (ident == "cos") return std::cos(arg);
                if (ident == "tan") return std::tan(arg);
                if (ident == "asin") return std::asin(arg);
                if (ident == "acos") return std::acos(arg);
                if (ident == "atan") return std::atan(arg);
                if (ident == "sqrt") return std::sqrt(arg);
                if (ident == "abs") return std::fabs(arg);
                if (ident == "exp") return std::exp(arg);
                if (ident == "log" || ident == "ln") return std::log(arg);
                if (ident == "log10") return std::log10(arg);
                if (ident == "floor") return std::floor(arg);
                if (ident == "ceil") return std::ceil(arg);
                throw std::runtime_error("Unsupported function: " + ident);
            }
            throw std::runtime_error("Unknown identifier in formula: " + ident);
        }
        return parseNumber();
    }

    double parseNumber() {
        skipSpaces();
        const std::size_t start = pos_;
        bool dotSeen = false;
        bool exponentSeen = false;

        if (peek() == '.') {
            dotSeen = true;
            ++pos_;
        }
        while (true) {
            const unsigned char c = static_cast<unsigned char>(peek());
            if (std::isdigit(c)) {
                ++pos_;
                continue;
            }
            if (peek() == '.' && !dotSeen && !exponentSeen) {
                dotSeen = true;
                ++pos_;
                continue;
            }
            if ((peek() == 'e' || peek() == 'E') && !exponentSeen) {
                exponentSeen = true;
                dotSeen = true;
                ++pos_;
                if (peek() == '+' || peek() == '-') ++pos_;
                continue;
            }
            break;
        }
        if (start == pos_) {
            throw std::runtime_error("Expected number in formula");
        }
        bool ok = false;
        const double value = text::toDouble(expr_.substr(start, pos_ - start), &ok);
        if (!ok) throw std::runtime_error("Invalid number in formula");
        return value;
    }

    std::string parseIdentifier() {
        skipSpaces();
        const std::size_t start = pos_;
        if (peek() == '\\') ++pos_;
        while (std::isalpha(static_cast<unsigned char>(peek())) || std::isdigit(static_cast<unsigned char>(peek())) || peek() == '_') ++pos_;
        return text::toLower(expr_.substr(start, pos_ - start));
    }

    void skipSpaces() {
        while (pos_ < expr_.size() && std::isspace(static_cast<unsigned char>(expr_[pos_]))) ++pos_;
    }

    char peek() const {
        return pos_ < expr_.size() ? expr_[pos_] : '\0';
    }

    bool match(char c) {
        if (peek() == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    bool startsImplicitMultiplication() {
        skipSpaces();
        const unsigned char c = static_cast<unsigned char>(peek());
        return c == '(' || c == '\\' || c == '.' || std::isdigit(c) != 0 || std::isalpha(c) != 0;
    }

    void require(char c) {
        skipSpaces();
        if (!match(c)) throw std::runtime_error(std::string("Expected '") + c + "' in formula");
    }

    std::string expr_;
    double x_ {0.0};
    std::size_t pos_ {0};
};

} // namespace

void FormulaEvaluator::validate(const std::string& expression) {
    if (text::trim(expression).empty()) {
        throw std::runtime_error("Formula expression cannot be empty");
    }
    (void)Parser(expression, 0.0).parse();
}

double FormulaEvaluator::evaluate(const std::string& expression, double x) {
    return Parser(expression, x).parse();
}

std::vector<Point> FormulaEvaluator::sample(const std::string& expression, double xMin, double xMax, int samples) {
    validate(expression);
    if (!std::isfinite(xMin) || !std::isfinite(xMax)) {
        throw std::runtime_error("Formula sampling range must be finite");
    }
    if (xMin > xMax) std::swap(xMin, xMax);
    if (samples < 2) samples = 2;

    std::vector<Point> points;
    points.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(samples - 1);
        const double x = xMin + (xMax - xMin) * t;
        const double y = evaluate(expression, x);
        if (std::isfinite(y)) points.push_back(Point{x, y});
    }
    if (points.size() < 2) {
        throw std::runtime_error("Formula produced fewer than two finite points in the requested range");
    }
    return points;
}

} // namespace plotapp
