#include <assert.h>

typedef struct {
    char *string;
} StructWithString;

void bad_deref(StructWithString *data)
{
    char *string = data->string;
}

void good_with_assert(StructWithString *data)
{
    assert(data != NULL);
    char *string = data->string;
}

void good_with_no_deref(StructWithString *data)
{
    good_with_assert(data);
}

int main(void)
{
    StructWithString data = { 0 };
    good_with_no_deref(&data);
    bad_deref(&data);
    return 0;
}
