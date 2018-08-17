#ifndef CSV_Included
#define CSV_Included

#include "linkedhashmap.h"
#include "grid.h"
#include <string>
#include <istream>

/* Type representing data read from a CSV file containing a header row. Access is
 * provided as csv[row][column], where column can be specified either by an integer
 * or as one of the column headers.
 */
class CSV {
public:
    /* Builds a CSV object from CSV data given in a stream. */
    explicit CSV(std::istream& source);
    explicit CSV(const std::string& filename);

    /* Basic accessors. */
    int numRows() const;   // Doesn't include header
    int numCols() const;
    
    /* Header information. */
    Vector<std::string> headers() const;

    /* Accessor proxy class. */
    class RowRef {
    public:
        std::string operator[] (int col) const;
        std::string operator[] (const std::string& colHeader) const;
    
    private:
        RowRef(const CSV* parent, int row);
        const CSV* mParent;
        int mRow;

        friend class CSV;
    };
    
    RowRef operator[] (int col) const;

private:
    /* The data. It's internally represented as a grid along with auxiliary column
     * header information.
     */
    Grid<std::string> mData;
    LinkedHashMap<std::string, int> mColumnHeaders;
};

#endif
