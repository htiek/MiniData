#include "JSON.h"
#include "error.h"
#include <unordered_map>
#include <vector>
#include <sstream>
#include <typeinfo>
#include <iomanip>
#include <string>
using namespace std;

/* Base internal type for JSON objects. */
class BaseJSON {
public:
    virtual ~BaseJSON() = default;

    /* Returns the type of this object. */
    JSON::Type type() const;

    /* Outputs this object to a stream. */
    // virtual void print(ostream& out) const = 0;

    /* Utility function to construct a new JSON object from a type and a list of
     * arguments. This is provided because BaseJSON is a friend of JSON and therefore
     * has access to its constructor.
     */
    template <typename Type, typename... Args>
    static JSON make(Args&&... args) {
        return JSON(make_shared<Type>(forward<Args>(args)...));
    }

protected:
    BaseJSON(JSON::Type type);

private:
    JSON::Type mType;
};

/* Type representing null. */
class NullJSON: public BaseJSON {
public:
    NullJSON(nullptr_t value);
    nullptr_t value() const;
};

/* Type representing a boolean. */
class BoolJSON: public BaseJSON {
public:
    BoolJSON(bool value);
    bool value() const;

private:
    bool mValue;
};

/* Type representing a number. */
class NumberJSON: public BaseJSON {
public:
    NumberJSON(double value);

    double value() const;

private:
    double mValue;
};

/* Type representing a string. */
class StringJSON: public BaseJSON {
public:
    StringJSON(const string& value);

    string value() const;

private:
    string mValue;
};

/* Intermediate type representing something with a size. */
class SizedJSON: public BaseJSON {
public:
    SizedJSON(JSON::Type type);

    virtual size_t size() const = 0;
};

/* Type representing an array. */
class ArrayJSON: public SizedJSON {
public:
    ArrayJSON(const vector<JSON>& elems);

    size_t size() const override;
    JSON operator[] (size_t index) const;

private:
    vector<JSON> mElems;
};

/* Type representing an object. */
class ObjectJSON: public SizedJSON {
public:
    ObjectJSON(const unordered_map<string, JSON>& elems);

    bool contains(const string& key) const;
    JSON operator[] (const string& key) const;
    size_t size() const override;

private:
    unordered_map<string, JSON> mElems;
};

/***************************************************************************/
/***********        Implementation of individual subtypes        ***********/
/***************************************************************************/
BaseJSON::BaseJSON(JSON::Type type) : mType(type) {

}
JSON::Type BaseJSON::type() const {
    return mType;
}

NullJSON::NullJSON(nullptr_t value) : BaseJSON(JSON::Type::NULLPTR_T) {
    if (value != nullptr) error("NullJSON constructed with non-null nullptr_t?");
}

nullptr_t NullJSON::value() const {
    return nullptr;
}

StringJSON::StringJSON(const string& value) : BaseJSON(JSON::Type::STRING), mValue(value) {

}

string StringJSON::value() const {
    return mValue;
}

NumberJSON::NumberJSON(double value) : BaseJSON(JSON::Type::NUMBER), mValue(value) {

}

double NumberJSON::value() const {
    return mValue;
}

BoolJSON::BoolJSON(bool value) : BaseJSON(JSON::Type::BOOLEAN), mValue(value) {

}

bool BoolJSON::value() const {
    return mValue;
}

SizedJSON::SizedJSON(JSON::Type type) : BaseJSON(type) {

}

ArrayJSON::ArrayJSON(const vector<JSON>& elems) : SizedJSON(JSON::Type::ARRAY), mElems(elems) {

}

size_t ArrayJSON::size() const {
    return mElems.size();
}

JSON ArrayJSON::operator[] (size_t index) const {
    if (index >= mElems.size()) {
        error("Index out of range: " + to_string(index) + ", but size is " + to_string(size()));
    }
    return mElems[index];
}

ObjectJSON::ObjectJSON(const unordered_map<string, JSON>& elems) : SizedJSON(JSON::Type::OBJECT), mElems(elems) {

}

bool ObjectJSON::contains(const string& key) const {
    return mElems.count(key);
}

JSON ObjectJSON::operator[](const string& key) const {
    if (!contains(key)) {
        error("Key " + key + " does not exist.");
    }
    return mElems.at(key);
}

size_t ObjectJSON::size() const {
    return mElems.size();
}



/***************************************************************************/
/***********          Implementation of JSON accessors           ***********/
/***************************************************************************/
namespace {
    /* Safely downcasts the underlying pointer type. */
    template <typename Target> shared_ptr<Target> as(shared_ptr<BaseJSON> base) {
        auto result = dynamic_pointer_cast<Target>(base);
        if (!result) {
            ostringstream result;
            result << "Wrong JSON type. Actual type is " << typeid(*base).name()
                   << ", which can't be converted to " << typeid(Target).name();
            error(result.str());
        }
        return result;
    }
}

JSON::JSON(shared_ptr<BaseJSON> impl) : mImpl(impl) {

}
JSON::Type JSON::type() const {
    return mImpl->type();
}
nullptr_t JSON::asNull() const {
    return as<NullJSON>(mImpl)->value();
}
bool JSON::asBoolean() const {
    return as<BoolJSON>(mImpl)->value();
}
double JSON::asNumber() const {
    return as<NumberJSON>(mImpl)->value();
}
string JSON::asString() const {
    return as<StringJSON>(mImpl)->value();
}
JSON JSON::operator [](size_t index) const {
    return (*as<ArrayJSON>(mImpl))[index];
}
size_t JSON::size() const {
    return as<SizedJSON>(mImpl)->size();
}
JSON JSON::operator [](const string& key) const {
    return (*as<ObjectJSON>(mImpl))[key];
}
bool JSON::contains(const string& key) const {
    return as<ObjectJSON>(mImpl)->contains(key);
}


/***************************************************************************/
/***********         Implementation of parsing routines          ***********/
/***************************************************************************/

/* String parsing just wraps things as an istringstream and forwards the call. */
JSON JSON::parse(const string& input) {
    istringstream converter(input);
    return parse(converter);
}

namespace {
    /* Utility function to report an error. This function is marked noreturn.
     *
     * TODO: Change StanfordCPPLib to mark error as [[ noreturn ]] to avoid the need
     * for this.
     */
    [[ noreturn ]] void parseError(const string& reason) {
        error("JSON Parse Error: " + reason);

        /* Unreachable; silences errors. */
        abort();
    }

    /* Utility function to peek at the next character of input. */
    char peek(istream& input) {
        int result = input.peek();
        if (result == EOF) parseError("Unexpected end of input.");

        return char(result);
    }

    /* Utility function to read the next character of input. */
    char get(istream& input) {
        char result;
        if (!input.get(result)) parseError("Unexpected end of input.");

        return result;
    }

    /* Utility function to confirm the next character matches a specific value. */
    void expect(istream& input, char ch) {
        char found = get(input);
        if (found != ch) parseError("Expected " + string(1, ch) + ", got " + string(1, found));
    }
    void expect(istream& input, const string& str) {
        for (char ch: str) {
            expect(input, ch);
        }
    }

    /* All of these parsing routines use the grammar specified on the JSON website
     * (https://www.json.org/). This is a top-down, recursive-descent parser.
     */
    JSON readObject(istream& input);
    JSON readElement(istream& input);
    JSON readArray(istream& input);
    double readNumber(istream& input);
    string readString(istream& input);

    nullptr_t readNull(istream& input) {
        expect(input, "null");
        return nullptr;
    }

    bool readBoolean(istream& input) {
        if (peek(input) == 't') {
            expect(input, "true");
            return true;
        } else if (peek(input) == 'f') {
            expect(input, "false");
            return false;
        } else {
            parseError("Can't parse a boolean starting with " + string(1, peek(input)));
        }
    }

    string readDigits(istream& input) {
        ostringstream result;

        /* There must be at least one digit. */
        char digit = get(input);
        if (!isdigit(digit)) {
            parseError("Expected a digit, got " + string(1, digit));
        }

        result << digit;

        /* If that digit was a zero, we're done. Otherwise, keep reading characters until
         * we hit something that isn't a digit.
         */
        if (digit != '0') {
            while (isdigit(peek(input))) {
                result << get(input);
            }
        }

        return result.str();
    }

    string readInt(istream& input) {
        ostringstream result;

        /* There could potentially be a minus sign. */
        if (peek(input) == '-') {
            result << get(input);
        }

        result << readDigits(input);
        return result.str();
    }

    string readFrac(istream& input) {
        /* If the next character isn't a dot, there's nothing to read. */
        if (peek(input) != '.') return "";

        /* Otherwise, we should see a dot, then a series of digits. */
        ostringstream result;
        result << get(input);
        result << readDigits(input);
        return result.str();
    }

    string readExp(istream& input) {
        /* If the next character isn't e or E, there's nothing to read. */
        if (peek(input) != 'E' && peek(input) != 'e') return "";

        ostringstream result;
        result << get(input);

        /* There may optionally be a sign. */
        if (peek(input) == '+' || peek(input) == '-') {
            result << get(input);
        }

        /* Now, read some digits. */
        result << readDigits(input);

        return result.str();
    }

    double readNumber(istream& input) {
        auto intPart  = readInt(input);
        auto fracPart = readFrac(input);
        auto expPart  = readExp(input);

        string number = intPart + fracPart + expPart;
        istringstream extractor(number);

        double result;
        if (extractor >> result, !extractor) {
            parseError("Successfully parsed " + number + " from input, but couldn't interpret it as a double.");
        }

        char leftover;
        if (extractor >> leftover) {
            parseError("Successfully parsed " + number + " from input, but when converting it, found extra character " + string(1, leftover));
        }

        return result;
    }

    JSON readValue(istream& input) {
        /* Determine what to read based on the next character of input. */
        char next = peek(input);

        if (next == '{') return readObject(input);
        if (next == '[') return readArray(input);
        if (next == '"') return BaseJSON::make<StringJSON>(readString(input));
        if (next == '-' || isdigit(next)) return BaseJSON::make<NumberJSON>(readNumber(input));
        if (next == 't' || next == 'f') return BaseJSON::make<BoolJSON>(readBoolean(input));
        if (next == 'n') return BaseJSON::make<NullJSON>(readNull(input));

        parseError("Not sure how to handle value starting with character " + string(1, next));
    }

    string readString(istream& input) {
        string result;

        expect(input, '"');

        /* Keep reading characters as we find them. */
        while (true) {
            char next = get(input);

            /* We're done if this is a close quote. */
            if (next == '"') return result;

            /* If this isn't an escape sequence, just append it. */
            if (next != '\\') result += next;

            /* Otherwise, read it as an escape. */
            else {
                char escaped = get(input);
                if      (escaped == '"')  result += '"';
                else if (escaped == '\\') result += '\\';
                else if (escaped == '/')  result += '/';
                else if (escaped == 'b')  result += '\b';
                else if (escaped == 'n')  result += '\n';
                else if (escaped == 'r')  result += '\r';
                else if (escaped == 't')  result += '\t';
                else if (escaped == 'u') {
                    // TODO: PROCESS UNICODE ESCAPE
                } else parseError("Unknown escape sequence: \\" + string(1, escaped));
            }
        }
    }

    using Member = unordered_map<string, JSON>::value_type;
    Member readMember(istream& input) {
        input >> ws;
        auto key = readString(input);
        input >> ws;

        expect(input, ':');

        auto value = readElement(input);

        return { key, value };
    }

    JSON readArray(istream& input) {
        expect(input, '[');

        vector<JSON> elems;

        /* Edge case: This could be an empty array. */
        if (peek(input) == ']') {
            get(input); // Consume ']'
            return BaseJSON::make<ArrayJSON>(elems);
        }

        /* Otherwise, it's a nonempty list. */
        while (true) {
            elems.push_back(readElement(input));

            /* The next character should either be a comma or a close bracket. We stop
             * on a close bracket and continue on a comma.
             */
            char next = get(input);
            if (next == ']') return BaseJSON::make<ArrayJSON>(elems);
            if (next != ',') parseError("Expected , or ], got " + string(1, next));
        }
    }

    JSON readObject(istream& input) {
        expect(input, '{');

        unordered_map<string, JSON> elems;

        /* Edge case: This could be an empty object. */
        if (peek(input) == '}') {
            get(input); // Consume ']'
            return BaseJSON::make<ObjectJSON>(elems);
        }

        /* Otherwise, it's a nonempty list. */
        while (true) {
            auto result = elems.insert(readMember(input));
            if (!result.second) parseError("Duplicate key: " + result.first->first);

            /* The next character should either be a comma or a close brace. We stop
             * on a close brace and continue on a comma.
             */
            char next = get(input);
            if (next == '}') return BaseJSON::make<ObjectJSON>(elems);
            if (next != ',') parseError("Expected , or }, got " + string(1, next));
        }
    }

    JSON readElement(istream& input) {
        input >> ws;
        auto result = readValue(input);
        input >> ws;
        return result;
    }
}

/* Main parsing routine. */
JSON JSON::parse(istream& input) {
    auto result = readElement(input);

    /* Confirm there's nothing left in the stream. */
    char leftover;
    input >> leftover;
    if (input) parseError("Unexpected character found at end of stream: " + string(1, leftover));

    return result;
}
