#cython: language_level=3

import numpy as np
from numpy.typing import NDArray
from typing import Generator

from cython.operator cimport dereference as deref, postincrement as inc
from cpython cimport PyList_New
from libcpp cimport bool
from libc.math cimport round
from libc.stdint cimport int64_t, intptr_t, uint64_t

from orderbook cimport CBookL3, COrderMap, IOrders, COrder, CBookL3Item, IBookL3, IRBookL3, TCallbackL3, COrderlog, TOrderFunc

default_prec = 2

TOrderlog = np.dtype([
    ('ts_event', 'M8[ns]'),
    ('ts_recv', 'M8[ns]'),
    ('action', 'S1'),
    ('side', 'S1'),
    ('price', 'f8'),
    ('size', 'i8'),
    ('channel_id', 'i4'),
    ('order_id', 'u8'),
    ('flags', 'u1'),
    ('ts_in_delta', 'i4'),
    ('sequence', 'i4'),
    ('symbol', 'S45'),
    ('rtype', 'i4'),
    ('publisher_id', 'u4'),
    ('instrument_id', 'i4')
])

_TOrderlogRaw = np.dtype([
    ('ts_event', 'i8'),
    ('ts_recv', 'i8'),
    ('action', 'S1'),
    ('side', 'S1'),
    ('price', 'f8'),
    ('size', 'i8'),
    ('channel_id', 'i4'),
    ('order_id', 'u8'),
    ('flags', 'u1'),
    ('ts_in_delta', 'i4'),
    ('sequence', 'i4'),
    ('symbol', 'S45'),
    ('rtype', 'i4'),
    ('publisher_id', 'u4'),
    ('instrument_id', 'i4')
])


cdef public double firstPriceL3(CBookL3 &Book):
    """returns the first (minimum) price from the sorted L3-orderbook"""
    return deref(Book.begin()).first if not Book.empty() else 0

cdef public int64_t firstSizeL3(CBookL3 &Book):
    """returns size for the first (minimum) price level from the sorted L3-orderbook"""
    return getSizeL3(deref(Book.begin()).second) if not Book.empty() else 0


cdef public double lastPriceL3(CBookL3 &Book):
    """returns the last (maximum) price from the sorted L3-orderbook"""
    return deref(Book.rbegin()).first if not Book.empty() else 0

cdef public int64_t lastSizeL3(CBookL3 &Book):
    """returns size for the last (maximum) price level from the sorted L3-orderbook"""
    return getSizeL3(deref(Book.rbegin()).second) if not Book.empty() else 0


cdef public bool existOrderL3(CBookL3 &Book, double price, uint64_t OrderID):
    """check for order existence by price and OrderID"""

    cdef COrderMap Orders
    if existPriceL3(Book, price):
        Orders = Book[price]
        return Orders.find(OrderID) != Orders.end()
    else:
        return False


cdef public bool existPriceL3(CBookL3 &Book, double price):
    """Check for existence of price level in the L3-orderbook"""
    return Book.find(price) != Book.end()


cdef void addOrderL3(CBookL3 &Book, double price, uint64_t orderID, int64_t size):
    """Insert new order into the orderbook"""

    cdef COrderMap Orders

    if existPriceL3(Book, price):
        Book[price].insert(COrder(orderID, size))
    else:
        Orders.insert(COrder(orderID, size))
        Book.insert(CBookL3Item(price, Orders))


cdef bool delOrderL3(CBookL3 &Book, double price, uint64_t orderID):
    """Delete one order from the orderbook"""

    if existPriceL3(Book, price):
        Book[price].erase(orderID)

        # drop price row if no orders available
        if Book[price].size() == 0:
            Book.erase(price)
        return True
    return False


cdef bool delOrderByID(CBookL3 &Book, uint64_t orderID):
    """Delete one order from the orderbook by id when the previous price is unknown"""
    cdef IBookL3 itBook = Book.begin()

    while itBook != Book.end():
        if deref(itBook).second.find(orderID) != deref(itBook).second.end():
            deref(itBook).second.erase(orderID)
            if deref(itBook).second.size() == 0:
                Book.erase(deref(itBook).first)
            return True
        inc(itBook)
    return False


cdef reduceOrderL3(CBookL3 &Book, double price, uint64_t orderID, int64_t size):
    """Reduce one order in the orderbook by Databento cancel size"""

    cdef int64_t newSize = 0
    if existPriceL3(Book, price):
        newSize = Book[price][orderID] - size
        Book[price][orderID] = newSize if newSize > 0 else 0

        # drop order if fully canceled
        if Book[price][orderID] == 0:
            Book[price].erase(orderID)

        # drop price row if no orders available
        if Book[price].size() == 0:
            Book.erase(price)


cdef modifyOrderL3(CBookL3 &Book, double price, uint64_t orderID, int64_t size):
    """Replace one order's current price/size with Databento modify values"""

    if size <= 0:
        delOrderL3(Book, price, orderID)
        delOrderByID(Book, orderID)
        return

    if not delOrderL3(Book, price, orderID):
        delOrderByID(Book, orderID)
    addOrderL3(Book, price, orderID, size)


cdef public void updateBookL3(CBookL3 &Book, const COrderlog &rec):
    """Insert/update/delete the orderbook by the new L3-orderlog record"""
    if rec.action == b'A' and rec.price > 0 and rec.size > 0:
        addOrderL3(Book, rec.price, rec.order_id, rec.size)
    elif rec.action == b'C':
        reduceOrderL3(Book, rec.price, rec.order_id, rec.size)
    elif rec.action == b'M' and rec.price > 0:
        modifyOrderL3(Book, rec.price, rec.order_id, rec.size)


cdef public void updateBooksL3(CBookL3 &Bid, CBookL3 &Ask, const COrderlog &rec):
    """Update the both (Bid or Ask) orderbooks by the new L3-orderlog record"""
    if rec.action == b'R':
        Bid.clear()
        Ask.clear()
    elif rec.side == b'B':
        updateBookL3(Bid, rec)
    elif rec.side == b'A':
        updateBookL3(Ask, rec)


cdef public int getCountL3(COrderMap &Orders):
    """Returns total count of the orders at the level"""
    return Orders.size()


cdef public int64_t getSizeL3(COrderMap &Orders):
    """Calculates total size of all orders at the level"""
    cdef:
        IOrders itOrders = Orders.begin()
        int64_t total_size = 0

    while itOrders != Orders.end():
        total_size += deref(itOrders).second
        inc(itOrders)
    return total_size


def parseL3(Orderlog, intptr_t callback_addr, **kwargs) -> list:
    """Process the orderlog by callback and returns list of its results"""

    cdef:
        const COrderlog[::1] records = np.ascontiguousarray(Orderlog).view(_TOrderlogRaw)
        Py_ssize_t length = 0
        list lst = PyList_New(length)
        COrderlog rec
        CBookL3 Bid, Ask
        TCallbackL3 callback = <TCallbackL3>callback_addr
        object result
        int i = 0

    # iterate over orderlog rows, build orderbook and run callback for each L3 row
    for rec in records:
        updateBooksL3(Bid, Ask, rec)

        # run callback on each L3 row, store valid result to list
        if callback_addr != 0:
            result = callback(i, rec, Bid, Ask, kwargs)
            if result is not None:
                lst.append(result)
        i += 1

    return lst


def gparseL3(Orderlog, intptr_t callback_addr, **kwargs) -> Generator[any]:
    """Process the orderlog by callback and returns list of its results"""

    cdef:
        const COrderlog[::1] records = np.ascontiguousarray(Orderlog).view(_TOrderlogRaw)
        CBookL3 Bid, Ask
        TCallbackL3 callback = <TCallbackL3>callback_addr
        object result
        Py_ssize_t i = 0

    # iterate over orderlog rows, build orderbook and run callback for each L3 row
    for rec in records:
        updateBooksL3(Bid, Ask, rec)

        # run callback on each L3 row, yield valid result
        if callback_addr != 0:
            result = callback(i, rec, Bid, Ask, kwargs)
            if result is not None:
                yield result
        i += 1


cdef cbSnapL3(int index, COrderlog &rec, CBookL3 &Bid, CBookL3 &Ask, dict kwargs):
    """Callback for L3 orderbook snapshot, returns dicts of Ask and Bid orderbook snapshots (very slow)"""
    cdef:
        IBookL3 itAsk = Ask.begin()
        IRBookL3 itBid = Bid.rbegin()
        dict A, B
        int h, height = kwargs.get('height', 10)

    A = {}
    B = {}
    h = 0
    while itAsk != Ask.end() and h < height:
        A[deref(itAsk).first] = deref(itAsk).second
        inc(itAsk)
        h += 1

    h = 0
    while itBid != Bid.rend() and h < height:
        B[deref(itBid).first] = deref(itBid).second
        inc(itBid)
        h += 1

    return B, A

def aSnapL3() -> np.int64:
    """Address of the cbSnapL3 callback"""
    return <intptr_t>cbSnapL3


cdef cbBidAsk(int index, COrderlog &rec, CBookL3 &Bid, CBookL3 &Ask, dict kwargs):
    """Callback for L3 orderbook bidask"""
    cdef:
        IBookL3 itAsk = Ask.begin()
        IRBookL3 itBid = Bid.rbegin()
        double bid = lastPriceL3(Bid), ask = firstPriceL3(Ask)

    if (bid > 0) and (ask > 0) and (ask > bid):
        return bid, ask

def aBidAsk() -> np.int64:
    """Address of the cbBidAsk callback"""
    return <intptr_t>cbBidAsk