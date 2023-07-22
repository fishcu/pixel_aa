#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    // Test the strlen function
    char str[] = "Hello, world!";
    int length = strlen(str);
    printf("Length of the string: %d\n", length);

    // Test the strcmp function
    char str1[] = "apple";
    char str2[] = "banana";
    int result = strcmp(str1, str2);
    if (result < 0) {
        printf("'%s' comes before '%s' in dictionary order.\n", str1, str2);
    } else if (result > 0) {
        printf("'%s' comes after '%s' in dictionary order.\n", str1, str2);
    } else {
        printf("'%s' and '%s' are equal.\n", str1, str2);
    }

    // Test the malloc function
    int* numbers = (int*)malloc(5 * sizeof(int));
    if (numbers != NULL) {
        for (int i = 0; i < 5; i++) {
            numbers[i] = i + 1;
        }
        printf("Dynamic array elements: ");
        for (int i = 0; i < 5; i++) {
            printf("%d ", numbers[i]);
        }
        printf("\n");
        free(numbers);  // Remember to free allocated memory after use.
    } else {
        printf("Memory allocation failed.\n");
    }

    return 0;
}
