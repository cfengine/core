#include <assert.h>

typedef struct {
    char *string;
} StructWithString;

void good_with_assert(StructWithString *data)
{
    assert(data != NULL);
    char *string = data->string;
}

void good_with_no_deref(StructWithString *data)
{
    good_with_assert(data);
}

void good_with_if(StructWithString *data)
{
    if (data != NULL)
    {
        char *string = data->string;
    }
}

void bad_deref(StructWithString *data)
{
    char *string = data->string;
}

int main(void)
{
    StructWithString *data = NULL;
    good_with_no_deref(data);      // Doesn't dereference, so no problem
    good_with_assert(data);        // Assert will detect our error
    good_with_if(data);            // Works with NULL pointers
    bad_deref(data);               // Blows up - will be detected in alert
    return 0;
}
