#include "JSON.h"
#include "Unicode.h"
#include "error.h"
#include <unordered_map>
#include <vector>
#include <sstream>
#include <typeinfo>
#include <iomanip>
#include <string>
#include <iterator>
using namespace std;

/* Base internal type for JSON objects. */
class BaseJSON {
public:
    virtual ~BaseJSON() = default;

    /* Returns the type of this object. */
    JSON::Type type() const;

    /* Outputs this object to a stream. */
    virtual void print(ostream& out) const = 0;

protected:
    BaseJSON(JSON::Type type);

private:
    JSON::Type mType;
};

/* Iterator support. This generator type is used to produce elements as a stream. */
class JSONSource {
public:
    virtual ~JSONSource() = default;
    
    virtual void advance() = 0;
    virtual bool finished() const = 0;
    
    virtual const JSON& current() const = 0;
    
    /* Constructs a const_iterator from a shared_ptr. */
    static JSON::const_iterator make(shared_ptr<JSONSource> impl) {
        return impl;
    }
};

/* Type representing null. */
class NullJSON: public BaseJSON {
public:
    NullJSON(nullptr_t value);
    nullptr_t value() const;
    
    void print(ostream& out) const override;
};

/* Type representing a boolean. */
class BoolJSON: public BaseJSON {
public:
    BoolJSON(bool value);
    bool value() const;
    
    void print(ostream& out) const override;

private:
    bool mValue;
};

/* Type representing a number. */
class NumberJSON: public BaseJSON {
public:
    NumberJSON(double value);
    double value() const;
    
    void print(ostream& out) const override;

private:
    double mValue;
};

/* Type representing a string. */
class StringJSON: public BaseJSON {
public:
    StringJSON(const string& value);
    string value() const;
    
    void print(ostream& out) const override;

private:
    string mValue;
};

/* Intermediate type representing something that can be iterated over. */
class IterableJSON: public BaseJSON {
public:
    IterableJSON(JSON::Type type);

    virtual size_t size() const = 0;
    virtual shared_ptr<JSONSource> source() const = 0;
};

/* Type representing an array. */
class ArrayJSON: public IterableJSON {
public:
    ArrayJSON(const vector<JSON>& elems);

    size_t size() const override;
    JSON operator[] (size_t index) const;
    
    void print(ostream& out) const override;
    shared_ptr<JSONSource> source() const override;

private:
    vector<JSON> mElems;
};

/* Type representing an object. */
class ObjectJSON: public IterableJSON {
public:
    ObjectJSON(const unordered_map<string, JSON>& elems);

    bool contains(const string& key) const;
    JSON operator[] (const string& key) const;
    size_t size() const override;
    
    void print(ostream& out) const override;
    shared_ptr<JSONSource> source() const override;

private:
    unordered_map<string, JSON> mElems;
};

/***************************************************************************/
/***********        Implementation of individual subtypes        ***********/
/***************************************************************************/

namespace {
    /* Utility printing routine to output a string. */
    void printString(ostream& out, const string& str) {
        out << '"';
    
        istringstream extractor(str);
        while (extractor.peek() != EOF) {
            char32_t ch = readChar(extractor);
            
            /* See how we need to encode this character. */
            if      (ch == '"')  out << "\\\"";
            else if (ch == '\\') out << "\\\\";
            else if (ch == '/')  out << "\\/";
            else if (ch == '\b') out << "\\b";
            else if (ch == '\n') out << "\\n";
            else if (ch == '\r') out << "\\r";
            else if (ch == '\t') out << "\\t";
            else if (ch >= 0x20 && ch <= 0x7F) out << char(ch);
            else out << utf16EscapeFor(ch);
        }
        
        out << '"';
    }
}

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

void NullJSON::print(ostream& out) const {
    out << "null";
}

StringJSON::StringJSON(const string& value) : BaseJSON(JSON::Type::STRING), mValue(value) {

}

string StringJSON::value() const {
    return mValue;
}

void StringJSON::print(ostream& out) const {
    printString(out, mValue);
}

NumberJSON::NumberJSON(double value) : BaseJSON(JSON::Type::NUMBER), mValue(value) {

}

double NumberJSON::value() const {
    return mValue;
}

void NumberJSON::print(ostream& out) const {
    out << mValue;
}

BoolJSON::BoolJSON(bool value) : BaseJSON(JSON::Type::BOOLEAN), mValue(value) {

}

bool BoolJSON::value() const {
    return mValue;
}

void BoolJSON::print(ostream& out) const {
    out << (mValue? "true" : "false");
}

IterableJSON::IterableJSON(JSON::Type type) : BaseJSON(type) {

}

ArrayJSON::ArrayJSON(const vector<JSON>& elems) : IterableJSON(JSON::Type::ARRAY), mElems(elems) {

}

size_t ArrayJSON::size() const {
    return mElems.size();
}

shared_ptr<JSONSource> ArrayJSON::source() const {
    /* Source just wraps a pair of iterators. */
    class VectorJSONSource: public JSONSource {
    public:
        VectorJSONSource(vector<JSON>::const_iterator curr, vector<JSON>::const_iterator end)
          : mCurr(curr), mEnd(end) {
          
        }
        
        void advance() override {
            ++mCurr;
        }
        bool finished() const override {
            return mCurr == mEnd;
        }
    
        const JSON& current() const override {
            return *mCurr;
        }
    
    private:
        vector<JSON>::const_iterator mCurr, mEnd;
    };
    
    return make_shared<VectorJSONSource>(mElems.begin(), mElems.end());
}

JSON ArrayJSON::operator[] (size_t index) const {
    if (index >= mElems.size()) {
        error("Index out of range: " + to_string(index) + ", but size is " + to_string(size()));
    }
    return mElems[index];
}

void ArrayJSON::print(ostream& out) const {
    out << '[';
    for (size_t i = 0; i < mElems.size(); i++) {
        out << mElems[i] << (i + 1 == mElems.size()? "" : ",");
    }
    out << ']';
}

ObjectJSON::ObjectJSON(const unordered_map<string, JSON>& elems) : IterableJSON(JSON::Type::OBJECT), mElems(elems) {

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

void ObjectJSON::print(ostream& out) const {
    out << '{';
    for (auto itr = mElems.begin(); itr != mElems.end(); ++itr) {
        printString(out, itr->first);
        out << ":" << itr->second;
        if (next(itr) != mElems.end()) {
            out << ",";
        }
    }
    out << '}';
}

shared_ptr<JSONSource> ObjectJSON::source() const {
    /* Source wraps a pair of iterators and stores a JSON object representing the
     * current string.
     */
    class MapJSONSource: public JSONSource {
    public:
        MapJSONSource(unordered_map<string, JSON>::const_iterator curr,
                      unordered_map<string, JSON>::const_iterator end)
          : mCurr(curr), mEnd(end), mStaged(nullptr) {
            if (mCurr != mEnd) {
                mStaged = JSON(mCurr->first);
            }
        }
        
        void advance() override {
            ++mCurr;
            if (mCurr != mEnd) {
                mStaged = JSON(mCurr->first);
            }
        }
        bool finished() const override {
            return mCurr == mEnd;
        }
    
        const JSON& current() const override {
            return mStaged;
        }
    
    private:
        unordered_map<string, JSON>::const_iterator mCurr, mEnd;
        JSON mStaged;
    };
    
    return make_shared<MapJSONSource>(mElems.begin(), mElems.end());
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
    return as<IterableJSON>(mImpl)->size();
}
JSON JSON::operator [](const string& key) const {
    return (*as<ObjectJSON>(mImpl))[key];
}
bool JSON::contains(const string& key) const {
    return as<ObjectJSON>(mImpl)->contains(key);
}

JSON JSON::operator [](JSON key) const {
    /* Forward as appropriate. */
    if (key.type() == JSON::Type::NUMBER) return (*this)[key.asNumber()];
    if (key.type() == JSON::Type::STRING) return (*this)[key.asString()];
    
    error("Cannot use this JSON object as a key.");
    abort();
}

ostream& operator<< (ostream& out, JSON json) {
    json.mImpl->print(out);
    return out;
}

/***************************************************************************/
/***********       Implementation of JSON::const_iterator        ***********/
/***************************************************************************/
JSON::const_iterator::const_iterator() {
    // Leave mImpl uninitialized
}
JSON::const_iterator::const_iterator(shared_ptr<JSONSource> source) : mImpl(source) {

}
JSON::JSON(nullptr_t) : mImpl(make_shared<NullJSON>(nullptr)) {

}
JSON::JSON(bool value) : mImpl(make_shared<BoolJSON>(value)) {

}
JSON::JSON(double value) : mImpl(make_shared<NumberJSON>(value)) {

}
JSON::JSON(const string& value) : mImpl(make_shared<StringJSON>(value)) {

}
JSON::JSON(const vector<JSON>& elems) : mImpl(make_shared<ArrayJSON>(elems)) {

}
JSON::JSON(const unordered_map<string, JSON>& elems) : mImpl(make_shared<ObjectJSON>(elems)) {

}

/* We support a minimal equality comparison that makes any iterator compare equal to itself
 * and any two iterators at the end of the range compare equal.
 */
bool JSON::const_iterator::operator== (const_iterator rhs) const {
    /* Case 1: Both iterators are null. */
    if (!mImpl && !rhs.mImpl) return true;
    
    /* Case 2: We're null, they aren't. */
    else if (!mImpl) return rhs.mImpl->finished();
    
    /* Case 3: They're null, we aren't. */
    else if (!rhs.mImpl) return mImpl->finished();
    
    /* Case 4: Neither is null. */
    return mImpl == rhs.mImpl;
}

bool JSON::const_iterator::operator!= (const_iterator rhs) const {
    return !(*this == rhs);
}
    
JSON::const_iterator& JSON::const_iterator::operator++ () {
    mImpl->advance();
    return *this;
}

const JSON::const_iterator JSON::const_iterator::operator++ (int) {
    auto result = *this;
    ++*this;
    return result;
}

const JSON& JSON::const_iterator::operator* () const {
    return mImpl->current();
}
    
const JSON* JSON::const_iterator::operator-> () const {
    return &**this;
}

JSON::const_iterator JSON::begin() const {
    return JSONSource::make(as<IterableJSON>(mImpl)->source());
}
JSON::const_iterator JSON::end() const {
    return {};
}
JSON::const_iterator JSON::cbegin() const {
    return begin();
}
JSON::const_iterator JSON::cend() const {
    return end();
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

    /* Utility function to confirm the next character matches a specific value. */
    void expect(istream& input, char32_t ch) {
        char32_t found = readChar(input);
        if (found != ch) parseError("Expected " + toUTF8(ch) + ", got " + toUTF8(found));
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
        if (peekChar(input) == 't') {
            expect(input, "true");
            return true;
        } else if (peekChar(input) == 'f') {
            expect(input, "false");
            return false;
        } else {
            parseError("Can't parse a boolean starting with " + toUTF8(peekChar(input)));
        }
    }
    
    bool isDigit(char32_t ch) {
        return ch >= '0' && ch <= '9';
    }

    string readDigits(istream& input) {
        ostringstream result;

        /* There must be at least one digit. */
        char32_t digit = readChar(input);
        if (!isDigit(digit)) {
            parseError("Expected a digit, got " + string(1, digit));
        }

        result << toUTF8(digit);

        /* If that digit was a zero, we're done. Otherwise, keep reading characters until
         * we hit something that isn't a digit.
         */
        if (digit != '0') {
            while (isDigit(peekChar(input))) {
                result << toUTF8(readChar(input));
            }
        }

        return result.str();
    }

    string readInt(istream& input) {
        ostringstream result;

        /* There could potentially be a minus sign. */
        if (peekChar(input) == '-') {
            result << toUTF8(readChar(input));
        }

        result << readDigits(input);
        return result.str();
    }

    string readFrac(istream& input) {
        /* If the next character isn't a dot, there's nothing to read. */
        if (peekChar(input) != '.') return "";

        /* Otherwise, we should see a dot, then a series of digits. */
        ostringstream result;
        result << toUTF8(readChar(input));
        result << readDigits(input);
        return result.str();
    }

    string readExp(istream& input) {
        /* If the next character isn't e or E, there's nothing to read. */
        if (peekChar(input) != 'E' && peekChar(input) != 'e') return "";

        ostringstream result;
        result << toUTF8(readChar(input));

        /* There may optionally be a sign. */
        if (peekChar(input) == '+' || peekChar(input) == '-') {
            result << toUTF8(readChar(input));
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
        char32_t next = peekChar(input);

        if (next == '{') return readObject(input);
        if (next == '[') return readArray(input);
        if (next == '"') return JSON(readString(input));
        if (next == '-' || isdigit(next)) return JSON(readNumber(input));
        if (next == 't' || next == 'f') return JSON(readBoolean(input));
        if (next == 'n') return JSON(readNull(input));

        parseError("Not sure how to handle value starting with character " + toUTF8(next));
    }

    string readString(istream& input) {
        string result;

        expect(input, '"');

        /* Keep reading characters as we find them. */
        while (true) {
            char32_t next = readChar(input);
            
            /* Only a certain character range is valid. */
            if (next < 0x20 || next > 0x10FFFF) parseError("Illegal character: " + toUTF8(next));

            /* We're done if this is a close quote. */
            if (next == '"') return result;

            /* If this isn't an escape sequence, just append it. */
            if (next != '\\') result += next;

            /* Otherwise, read it as an escape. */
            else {
                char32_t escaped = readChar(input);
                if      (escaped == '"')  result += '"';
                else if (escaped == '\\') result += '\\';
                else if (escaped == '/')  result += '/';
                else if (escaped == 'b')  result += '\b';
                else if (escaped == 'n')  result += '\n';
                else if (escaped == 'r')  result += '\r';
                else if (escaped == 't')  result += '\t';
                else if (escaped == 'u') {
                    input.unget();
                    input.unget();
                    result += toUTF8(readUTF16EscapedChar(input));
                } else parseError("Unknown escape sequence: \\" + toUTF8(escaped));
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
        if (peekChar(input) == ']') {
            readChar(input); // Consume ']'
            return JSON(elems);
        }

        /* Otherwise, it's a nonempty list. */
        while (true) {
            elems.push_back(readElement(input));

            /* The next character should either be a comma or a close bracket. We stop
             * on a close bracket and continue on a comma.
             */
            char32_t next = readChar(input);
            if (next == ']') return JSON(elems);
            if (next != ',') parseError("Expected , or ], got " + toUTF8(next));
        }
    }

    JSON readObject(istream& input) {
        expect(input, '{');

        unordered_map<string, JSON> elems;

        /* Edge case: This could be an empty object. */
        if (peekChar(input) == '}') {
            readChar(input); // Consume ']'
            return JSON(elems);
        }

        /* Otherwise, it's a nonempty list. */
        while (true) {
            auto result = elems.insert(readMember(input));
            if (!result.second) parseError("Duplicate key: " + result.first->first);

            /* The next character should either be a comma or a close brace. We stop
             * on a close brace and continue on a comma.
             */
            char32_t next = readChar(input);
            if (next == '}') return JSON(elems);
            if (next != ',') parseError("Expected , or }, got " + toUTF8(next));
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
