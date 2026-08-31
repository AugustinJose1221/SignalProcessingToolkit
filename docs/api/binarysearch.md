# binarysearch

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Binary search. Declared in `ffitt/util/binarysearch.h`.

[Back to the index](../API.md) | [How the util modules work](../../ffitt/util/README.md)

## Functions

### `binarysearch_get_index`

```c
uint32_t binarysearch_get_index(real_t* data, real_t value, uint32_t size);
```

Give the index of the first value of the list that is not less than the
given value. The values of the list must rise.

The result is always an index that the caller can use. If every value of the
list is less than the given value, the result is the index of the last
value. Thus a caller that reads the list at the result never reads memory
after the end of the list.
