/**
 * @file
 * @brief Bit array data type.
 *
 * Just contains the operations required by los.cc
 * for the moment.
**/

#ifndef BITARY_H
#define BITARY_H

class bit_array
{
public:
    bit_array(unsigned long size = 0);
    ~bit_array();

    void reset();

    bool get(unsigned long index) const;
    void set(unsigned long index, bool value = true);

    bit_array& operator |= (const bit_array& other);
    bit_array& operator &= (const bit_array& other);
    bit_array  operator & (const bit_array& other) const;

protected:
    unsigned long size;
    int nwords;
    unsigned long *data;
};

#endif
