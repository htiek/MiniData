#ifndef JSON_Included
#define JSON_Included

#include <memory>
#include <istream>
#include <ostream>

/* Type representing a value represented in JSON format. */
class JSON {
public:
    /* Parses a piece of text into JSON format. */
    static JSON parse(std::istream& input);
    static JSON parse(const std::string& input);

    /* Enumeration representing what type of object this is. */
    enum class Type {
        OBJECT,
        ARRAY,
        STRING,
        NUMBER,
        BOOLEAN,
        NULLPTR_T
    };

    /* Returns the type of this object. */
    Type type() const;

    /* Accessors. All of these functions will raise an error() if the underlying type
     * is incorrect.
     */
    double         asNumber()  const;
    bool           asBoolean() const;
    std::nullptr_t asNull()    const;
    std::string    asString()  const;

    /* Array accessors. Again, these will raise error()s if the underlying type is
     * incorrect.
     */
    JSON operator[] (std::size_t index) const;

    /* Object accessors. As usual, these raise error()s if the underlying type is
     * incorrect.
     */
    JSON operator[] (const std::string& field) const;
    bool contains(const std::string& fieldName) const;

    /* Shared between arrays and objects. */
    std::size_t size() const;

    class key_iterator;
    key_iterator begin() const;
    key_iterator end() const;

private:
    friend class BaseJSON;
    std::shared_ptr<class BaseJSON> mImpl;

    JSON(std::shared_ptr<class BaseJSON> impl);
};

std::ostream& operator<< (std::ostream& out, JSON json);

#endif
