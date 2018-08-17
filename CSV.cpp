#include "CSV.h"
#include "strlib.h"
#include "error.h"
#include <sstream>
#include <fstream>
using namespace std;

namespace {
    /* Reads a single CSV token from a source. Each token either
     *
     *  1. does not start with a quote, in which case we read up until the first comma, or
     *  2. starts with a quote, in which case we read to the upcoming close quote, watching for
     *     escaped quotes along the way.
     *
     * Empty entries are acceptable.
     */
    string readOneTokenFrom(istream& input) {
        /* Edge case: empty entries are fine. */
        if (input.peek() == ',') return "";
        
        /* If we don't start with a quote, read up until we do. */
        if (input.peek() != '"') {
            string result;           
            while (true) {
                int ch = input.peek();
                if (ch == EOF) return result;
                if (ch == ',') return result;
                result += char(input.get());
            }
        }
        
        /* We are looking a quoted string. Keep reading characters, keeping in mind that a close
         * quote might not actually be the end-of-string marker.
         */
        input.get(); // Skip quotation mark
        
        string result;
        while (true) {
            int ch = input.get();
            
            if (ch == EOF) error("Unterminated string literal.");
            else if (ch != '"') result += char(ch);
            else {
                int next = input.peek();
                if (next == EOF || next == ',') return result; // End of token
                else if (next == '"') {
                    /* Consume this character so we don't process it twice. */
                    input.get();
                    result += '"';
                } else error("Unexpected character found after quote.");
            }
        }
    }

    /* Tokenizes a line from a CSV file, returning a list of tokens within that line. */
    Vector<string> tokenize(const string& line) {
        /* Edge case: we assume there are no empty lines even though in principle we could
         * envision a 0 x n data array. That likely just means something went wrong.
         */
        if (line.empty()) error("Empty line in CSV file.");
    
        /* Convert to a stream to make it easier to treat the characters as though they're a stream. */
        istringstream input(line);
        
        Vector<string> result;
        while (true) {
            result += readOneTokenFrom(input);
            
            /* We should either see a comma or an EOF at this point. */
            if (input.peek() == EOF) return result;
            if (input.get()  != ',') error("Entries in CSV file aren't comma-separated?");
        }
    }

    /* Reads the first line of a CSV file, breaking it apart into headers. */
    LinkedHashMap<string, int> readHeaders(istream& input) {
        string line;
        if (!getline(input, line)) error("Could not read header row from CSV source.");
        
        LinkedHashMap<string, int> result;
        for (auto token: tokenize(line)) {
            if (result.containsKey(token)) error("Duplicate column header: " + token);
            
            int index = result.size();
            result.add(token, index);
        }
        
        return result;
    }
    
    /* Reads the body of a CSV file under the assumption that it has a certain number of
     * columns.
     */
    Grid<string> readBody(istream& input, int numCols) {
        /* We'll build the grid as a Vector<Vector<string>> and collapse it at the end. */
        Vector<Vector<string>> lines;
        for (string line; getline(input, line); ) {
            auto tokens = tokenize(line);
            if (tokens.size() != numCols) error("Lines have varying number of entries.");
            
            lines += tokens;
        }
        
        /* Flatten the list. */
        Grid<string> result(lines.size(), numCols);
        for (int row = 0; row < lines.size(); row++) {
            for (int col = 0; col < numCols; col++) {
                result[row][col] = lines[row][col];
            }
        }
        return result;
    }
}

CSV::CSV(istream& input) {
    mColumnHeaders = readHeaders(input);
    mData          = readBody(input, mColumnHeaders.size());
}

CSV::CSV(const string& filename) {
    ifstream input(filename);
    if (!input) error("Cannot open file " + filename);

    mColumnHeaders = readHeaders(input);
    mData          = readBody(input, mColumnHeaders.size());
}

int CSV::numRows() const {
    return mData.numRows();
}

int CSV::numCols() const {
    return mData.numCols();
}

Vector<string> CSV::headers() const {
    Vector<string> result;
    for (auto header: mColumnHeaders) {
        result += header;
    }
    return result;
}

CSV::RowRef CSV::operator[] (int row) const {
    if (row < 0 || row >= numRows()) error("Row out of range.");
    
    return RowRef(this, row);
}

CSV::RowRef::RowRef(const CSV* parent, int row) : mParent(parent), mRow(row) {

}

string CSV::RowRef::operator[] (int col) const {
    if (col < 0 || col >= mParent->numCols()) error("Column out of range.");
    
    return mParent->mData[mRow][col];
}
string CSV::RowRef::operator[] (const string& colHeader) const {
    if (!mParent->mColumnHeaders.containsKey(colHeader)) error("Column not found: " + colHeader);

    return (*this)[mParent->mColumnHeaders.get(colHeader)];
}
