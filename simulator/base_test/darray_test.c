#include <stdio.h>
#include <assert.h>
#include "dsa/darray.h"

// Debug printing macros
#define PRINT_INT_ARRAY(arr)                                      \
    do                                                            \
    {                                                             \
        int *darray_print_int_p;                                  \
        printf("[");                                              \
        darray_foreach(darray_print_int_p, arr)                   \
        {                                                         \
            printf("%d", *darray_print_int_p);                    \
            if (darray_print_int_p + 1 < &(arr).item[(arr).size]) \
                printf(", ");                                     \
        }                                                         \
        printf("]\n");                                            \
    } while (0)

#define PRINT_CHAR_ARRAY(arr)                    \
    do                                           \
    {                                            \
        char *darray_print_char_p;               \
        printf("\"");                            \
        darray_foreach(darray_print_char_p, arr) \
        {                                        \
            printf("%c", *darray_print_char_p);  \
        }                                        \
        printf("\"\n");                          \
    } while (0)

// Custom struct for testing
typedef struct
{
    int n, d;
} Fraction;

typedef darray(Fraction) darray_Fraction;

static void print_fraction_array(darray_Fraction arr)
{
    Fraction *f;
    printf("[");
    darray_foreach(f, arr)
    {
        printf("%d/%d", f->n, f->d);
        if (f + 1 < &(arr).item[(arr).size])
            printf(", ");
    }
    printf("]\n");
}

int main(void)
{
    printf("=== darray Test Suite Started ===\n\n");

    // -------------------------------------------------------------------------
    // 1. Lifecycle and basic insertion (int)
    // -------------------------------------------------------------------------
    printf("1. Testing darray(int) lifecycle and insertion\n");
    darray(int) arr = darray_new();
    assert(arr.item == NULL);
    assert(darray_size(arr) == 0);
    assert(darray_alloc(arr) == 0);
    assert(darray_empty(arr));

    darray_append(arr, 10);
    darray_append(arr, 20);
    darray_append(arr, 30);
    assert(darray_size(arr) == 3);
    assert(darray_item(arr, 0) == 10);
    assert(darray_item(arr, 1) == 20);
    assert(darray_item(arr, 2) == 30);

    PRINT_INT_ARRAY(arr); // [10, 20, 30]

    darray_prepend(arr, 5);
    assert(darray_item(arr, 0) == 5);
    assert(darray_size(arr) == 4);
    PRINT_INT_ARRAY(arr); // [5, 10, 20, 30]

    darray_insert(arr, 2, 15);
    assert(darray_item(arr, 2) == 15);
    assert(darray_size(arr) == 5);
    PRINT_INT_ARRAY(arr); // [5, 10, 15, 20, 30]

    darray_push(arr, 35);
    assert(darray_size(arr) == 6);
    assert(darray_item(arr, 5) == 35);

    // -------------------------------------------------------------------------
    // 2. Bulk insertion: darray_appends
    // -------------------------------------------------------------------------
    printf("\n2. Testing darray_appends\n");
    darray_appends(arr, 40, 45, 50);
    assert(darray_size(arr) == 9);
    assert(darray_item(arr, 8) == 50);
    PRINT_INT_ARRAY(arr); // [5,10,15,20,30,35,40,45,50]

    // -------------------------------------------------------------------------
    // 3. Traversal: forward and reverse
    // -------------------------------------------------------------------------
    printf("\n3. Testing darray_foreach and darray_foreach_reverse\n");
    printf("Forward traversal: ");
    int *p;
    darray_foreach(p, arr)
    {
        printf("%d ", *p);
    }
    printf("\n");

    printf("Reverse traversal: ");
    darray_foreach_reverse(p, arr)
    {
        printf("%d ", *p);
    }
    printf("\n");

    // -------------------------------------------------------------------------
    // 4. Removal: pop and remove
    // -------------------------------------------------------------------------
    printf("\n4. Testing darray_pop and darray_remove\n");
    assert(darray_pop(arr) == 50);
    assert(darray_size(arr) == 8);
    assert(darray_pop(arr) == 45);
    assert(darray_size(arr) == 7);

    // Test darray_pop_check on empty array (NULL for pointer types)
    darray(int) empty = darray_new();
    if (darray_size(empty) > 0)
    {
        printf("\npop[%d]\n", darray_pop(empty));
    }
    darray_free(empty);

    // Remove middle element
    darray_remove(arr, 2); // removes 15
    assert(darray_size(arr) == 6);
    assert(darray_item(arr, 2) == 20);
    PRINT_INT_ARRAY(arr); // [5,10,20,30,35,40]

    // Remove last element
    darray_remove(arr, 5);
    assert(darray_size(arr) == 5);
    assert(darray_item(arr, 4) == 35);

    // Remove first element
    darray_remove(arr, 0);
    assert(darray_item(arr, 0) == 10);
    PRINT_INT_ARRAY(arr); // [10,20,30,35]

    // -------------------------------------------------------------------------
    // 5. Multiple types: struct
    // -------------------------------------------------------------------------
    printf("\n5. Testing darray_Fraction with darray_appends\n");
    darray_Fraction fractions = darray_new();

    darray_appends(fractions, {1, 2}, {3, 4}, {5, 6});
    assert(darray_size(fractions) == 3);
    assert(fractions.item[0].n == 1 && fractions.item[0].d == 2);

    print_fraction_array(fractions); // [1/2, 3/4, 5/6]

    darray_free(fractions);

    // -------------------------------------------------------------------------
    // 6. String buffer operations: darray(char)
    // -------------------------------------------------------------------------
    printf("\n6. Testing darray(char) string operations\n");
    darray(char) str = darray_new();

    darray_append_lit(str, "Hello");
    PRINT_CHAR_ARRAY(str); // "Hello"

    darray_append_string(str, " World");
    PRINT_CHAR_ARRAY(str); // "Hello World"

    darray_prepend_lit(str, "Hi, ");
    PRINT_CHAR_ARRAY(str); // "Hi, Hello World"

    darray_from_lit(str, "Overwrite");
    PRINT_CHAR_ARRAY(str); // "Overwrite"

    darray_from_string(str, "Another String");
    PRINT_CHAR_ARRAY(str); // "Another String"

    darray_free(str);

    // -------------------------------------------------------------------------
    // 7. Size management: resize, realloc, make_room
    // -------------------------------------------------------------------------
    printf("\n7. Testing darray_resize and darray_make_room\n");
    darray(int) resize_arr = darray_new();

    darray_resize(resize_arr, 5);
    assert(darray_size(resize_arr) == 5);
    assert(darray_alloc(resize_arr) >= 5);
    for (int i = 0; i < 5; i++)
    {
        darray_item(resize_arr, i) = i * 10;
    }

    PRINT_INT_ARRAY(resize_arr); // [0,10,20,30,40]

    darray_resize0(resize_arr, 8); // grow and zero-fill
    assert(darray_size(resize_arr) == 8);
    assert(darray_item(resize_arr, 7) == 0);
    PRINT_INT_ARRAY(resize_arr); // [0,10,20,30,40,0,0,0]

    // Test darray_make_room
    int *room = darray_make_room(resize_arr, 10);
    assert(room != NULL);
    assert(darray_alloc(resize_arr) >= darray_size(resize_arr) + 10);
    (void)room;
    // Safe to write room[0..9]

    darray_free(resize_arr);

    // -------------------------------------------------------------------------
    // 8. Struct initialization with darray_init
    // -------------------------------------------------------------------------
    printf("\n8. Testing darray_init inside a struct\n");
    struct
    {
        darray(int) nums;
        int other_field;
    } container;

    darray_init(container.nums);
    container.other_field = 42;

    darray_append(container.nums, 100);
    assert(darray_size(container.nums) == 1);
    assert(darray_item(container.nums, 0) == 100);

    darray_free(container.nums);

    // -------------------------------------------------------------------------
    printf("\n=== All tests passed successfully! ===\n");
    return 0;
}