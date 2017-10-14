#include <stdio.h>
#include <time.h>

#include <scil.h>
#include <scil-util.h>

/*  We test the numbers 1 to 10 compressed with x bits
    x bits =
      1   not stored bit for fixed 1.0 + stored mantissa
      x-1 real mantissa bits
    so we have:
      x   possible mantissa values
      1   1.0
      2   1.0, 1.5
      3   1.0, 1.25, 1.5, 1.75

    We also know the sigbit algorithm should round
      to lower value for input <  (lower + upper)/2
      to upper value for input >= (lower + upper)/2
    as this minimizes the resulting relative error.
*/

size_t count           = 10;
float input_values[]  = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0,10.0 };
float expected_1bit[] = { 1.0, 2.0, 4.0, 4.0, 4.0, 8.0, 8.0, 8.0, 8.0, 8.0 };
float expected_2bit[] = { 1.0, 2.0, 3.0, 4.0, 6.0, 6.0, 8.0, 8.0, 8.0,12.0 };
float expected_3bit[] = { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0,10.0,10.0 };
float *expected_4bit  = input_values;

float expected_value(char sigbits, size_t i) {
    switch(sigbits) {
        case 1:
            return expected_1bit[i];
        case 2:
            return expected_2bit[i];
        case 3:
            return expected_3bit[i];
        case 4:
            return expected_4bit[i];
    }
}

int test_sigbits(char sigbits) {
    scil_user_hints_t hints;
    scilPr_initialize_user_hints(&hints);
    hints.significant_bits = sigbits;
    hints.force_compression_methods = "3";

    scil_context_t* context;
    scilPr_create_context(&context, SCIL_TYPE_FLOAT, 0, NULL, &hints);

    scil_dims_t dims;
    scilPr_initialize_dims_1d(&dims, count);

    size_t uncompressed_size = scilPr_get_dims_size(&dims, SCIL_TYPE_FLOAT);
    size_t compressed_size   = scilPr_get_compressed_data_size_limit(&dims, SCIL_TYPE_FLOAT);

    float* buffer_in  = (float*)malloc(uncompressed_size);
    byte* buffer_out   = (byte*)malloc(compressed_size);
    byte* buffer_tmp   = (byte*)malloc(compressed_size / 2);
    float* buffer_end = (float*)malloc(uncompressed_size);

    for(size_t i = 0; i < count; ++i){
        buffer_in[i] = input_values[i];
    }

    size_t out_size;
    scil_compress(buffer_out, compressed_size, buffer_in, &dims, &out_size, context);
    scil_decompress(SCIL_TYPE_FLOAT, buffer_end, &dims, buffer_out, out_size, buffer_tmp);

    int errors = 0;
    printf("#Testing sigbit algorithm with hint 'number of sigbits' = %d\n", sigbits);
    printf("#Input value,Expected value,Value after comp-decomp,Status\n");
    for (size_t i = 0; i < count; ++i) {
        float expected = expected_value(sigbits, i);
        printf("%f,%f,%f", buffer_in[i], expected, buffer_end[i]);
        if (buffer_end[i] != expected) {
            printf(",Error\n");
            errors++;
        }
        else
            printf(",Ok\n");
    }

    free(buffer_in);
    free(buffer_out);
    free(buffer_tmp);
    free(buffer_end);

    //scilPr_destroy_context(context);

    return errors;
}

int main(void) {
  int errors = 0;
  errors += test_sigbits(1);
  errors += test_sigbits(2);
  errors += test_sigbits(3);
  errors += test_sigbits(4);
  return errors;
}
