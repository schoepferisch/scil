#include <stdio.h>

#include <scil.h>

int main(void){

    size_t count = 10000;

    scil_user_hints_t hints;
    scilPr_initialize_user_hints(&hints);
    hints.absolute_tolerance = 0.005;
    hints.force_compression_methods = "1";

    scil_context_p context;
    scil_create_compression_context(&context, SCIL_TYPE_DOUBLE, 0, NULL, &hints);

    scil_dims dims;
    scil_init_dims_1d(&dims, count);

    size_t uncompressed_size = scil_get_data_size(SCIL_TYPE_DOUBLE, &dims);
    size_t compressed_size   = scil_compress_buffer_size_bound(SCIL_TYPE_DOUBLE, &dims);

    double* buffer_in  = (double*)malloc(uncompressed_size);
    byte* buffer_out   = (byte*)malloc(compressed_size);
    byte* buffer_tmp   = (byte*)malloc(compressed_size / 2);
    double* buffer_end = (double*)malloc(uncompressed_size);

    for(size_t i = 0; i < count; ++i){
        buffer_in[i] = ((double)rand()/RAND_MAX - 0.5) * 200;
    }

    size_t out_size;
    scil_compress(buffer_out, compressed_size, buffer_in, &dims, &out_size, context);
    scil_decompress(SCIL_TYPE_DOUBLE, buffer_end, &dims, buffer_out, out_size, buffer_tmp);

    printf("#Value,Value after comp-decomp,Difference\n");
    for (size_t i = 0; i < count; ++i) {
        printf("%f,%f,%f\n", buffer_in[i], buffer_end[i], buffer_end[i] - buffer_in[i]);
    }

    printf("#Buffer size,Buffer size after compression\n");
    printf("%lu,%lu\n", uncompressed_size, out_size);

    free(buffer_in);
    free(buffer_out);
    free(buffer_tmp);
    free(buffer_end);

    //scil_destroy_compression_context(&context);

    return 0;
}
