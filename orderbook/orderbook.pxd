#cython: language_level=3

from libcpp cimport bool
from libcpp.map cimport map
from libcpp.pair cimport pair
from libc.stdint cimport int32_t, int64_t, uint8_t, uint32_t, uint64_t

# The struct needs to be packed since numpy dtypes aren't aligned by default
# Must be byte-identical with the TOrderlog/TOrderlogDT dtypes
ctypedef public packed struct COrderlog:
    int64_t ts_event
    int64_t ts_recv
    char action
    char side
    double price
    int64_t size
    int32_t channel_id
    uint64_t order_id
    uint8_t flags
    int32_t ts_in_delta
    int32_t sequence
    char symbol[45]
    int32_t rtype
    uint32_t publisher_id
    int32_t instrument_id

# L3 orderbook map type definitions
ctypedef public map[uint64_t, int64_t] COrderMap   # orderid -> size map
ctypedef map[uint64_t, int64_t].iterator IOrders
ctypedef map[uint64_t, int64_t].reverse_iterator IROrders
ctypedef pair[uint64_t, int64_t] COrder

ctypedef public map[double, COrderMap] CBookL3  # price -> orders map
ctypedef map[double, COrderMap].iterator IBookL3
ctypedef map[double, COrderMap].reverse_iterator IRBookL3
ctypedef pair[double, COrderMap] CBookL3Item

# user callback function prototype (called by parse/parseL3() funcs for each update)
ctypedef object (*TCallbackL3)(int index, COrderlog &rec, CBookL3 &Bid, CBookL3 &Ask, dict kwargs)

# auxiliary functions to access the book
cdef public double firstPriceL3(CBookL3 &Book)
cdef public double lastPriceL3(CBookL3 &Book)
cdef public int64_t firstSizeL3(CBookL3 &Book)
cdef public int64_t lastSizeL3(CBookL3 &Book)

cdef public void addOrderL3(CBookL3 &Book, double price, uint64_t orderID, int64_t size)
cdef public bool existOrderL3(CBookL3 &Book, double price, uint64_t OrderID)
cdef public bool existPriceL3(CBookL3 &Book, double price)

cdef public int64_t getSizeL3(COrderMap &Orders)
cdef public int getCountL3(COrderMap &Orders)

ctypedef long (*TFunc)(COrderlog rec)
ctypedef int64_t (*TOrderFunc)(COrderMap &)

cdef public void updateBookL3(CBookL3 &Book, const COrderlog &rec)
cdef public void updateBooksL3(CBookL3 &Bid, CBookL3 &Ask, const COrderlog &rec)
