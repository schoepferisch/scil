// This file is part of SCIL.
//
// SCIL is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// SCIL is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with SCIL.  If not, see <http://www.gnu.org/licenses/>.

/*
 * This tool will compress / decompress a CSV
 */

#include <scil.h>
#include <scil-error.h>
#include <scil-option.h>
#include <scil-util.h>
#include <scil-config.h>
#include <scil-debug.h>

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <file-formats/scil-file-format.h>

#define DATA_SIZE_LIMIT 3 << 29

static int validate = 0;
static int verbose = 0;
static int compress = 0;
static int uncompress = 0;
static int cycle = 0;
static char * in_file = "";
static char * out_file = NULL;
static int compute_residual = 0;
static int use_chunks = 0;


static int use_max_value_as_fill_value = 0;
static int measure_time = 0;
static int print_hints = 0;

static char * in_file_format = "csv";
static char * out_file_format = "csv";

// data we process

static scil_dims_t dims;
static byte * input_data = NULL;
static byte * output_data = NULL;

static scil_file_plugin_t * in_plugin = NULL;
static scil_file_plugin_t * out_plugin = NULL;

int main(int argc, char ** argv){
  scil_context_t* ctx = NULL;
  scil_user_hints_t hints;
  scil_user_hints_t out_accuracy;

  printf("scil-compress (Git commit:%s)\ncompiler-options: %s\ncompiler-version: %s\n", GIT_VERSION, C_COMPILER_OPTIONS, C_COMPILER_VERSION);
  double fake_abstol_value = 0;
  double fake_finest_abstol_value = 0;

  int ret;

  scil_user_hints_initialize(&hints);
  option_help known_args[] = {
    {'i', "in_file", "Input file (file format depends on mode)", OPTION_REQUIRED_ARGUMENT, 's', & in_file},
    {'o', "out_file", "Output file (file format depends on mode)", OPTION_OPTIONAL_ARGUMENT, 's', & out_file},
    {'I', "in_file_format", "Input file format, list shows all available formats", OPTION_OPTIONAL_ARGUMENT, 's', & in_file_format},
    {'O', "out_file_format", "Output file format", OPTION_OPTIONAL_ARGUMENT, 's', & out_file_format},
    {'x', "decompress", "Infile is expected to be a binary compressed with this tool, outfile a CSV file",OPTION_FLAG, 'd', & uncompress},
    {'c', "compress", "Infile is expected to be a CSV file, outfile a binary",OPTION_FLAG, 'd' , & compress},
    {'R', "residual", "(for decompression) compute/output the residual error instead of the data", OPTION_FLAG, 'd', & compute_residual},
    {'t', "time", "Measure time for the operation.", OPTION_FLAG, 'd', & measure_time},
    {'V', "validate", "Validate the output", OPTION_FLAG, 'd', & validate},
    {'v', "verbose", "Increase the verbosity level", OPTION_FLAG, 'd', & verbose},
    {'H', "print-hints", "Print the effective hints", OPTION_FLAG, 'd', & print_hints},
    {0, "use-max-value-as-fill", "Check for the maximum value and use it as fill value", OPTION_FLAG, 'd',  & use_max_value_as_fill_value},
    {0, "hint-force_compression_methods", NULL,  OPTION_OPTIONAL_ARGUMENT, 's', & hints.force_compression_methods},
    {0, "hint-absolute-tolerance", NULL,  OPTION_OPTIONAL_ARGUMENT, 'F', & hints.absolute_tolerance},
    {0, "hint-relative_tolerance_percent", NULL,  OPTION_OPTIONAL_ARGUMENT, 'F', & hints.relative_tolerance_percent},
    {0, "hint-relative_err_finest_abs_tolerance", NULL,  OPTION_OPTIONAL_ARGUMENT, 'F', & hints.relative_err_finest_abs_tolerance},
    {0, "hint-significant_bits", NULL,  OPTION_OPTIONAL_ARGUMENT, 'd', & hints.significant_bits},
    {0, "hint-significant_digits", NULL,  OPTION_OPTIONAL_ARGUMENT, 'd', & hints.significant_digits},
    {0, "hint-comp-speed-unit", NULL,  OPTION_OPTIONAL_ARGUMENT, 'e', & hints.comp_speed.unit},
    {0, "hint-decomp-speed-unit", NULL,  OPTION_OPTIONAL_ARGUMENT, 'e', & hints.decomp_speed.unit},
    {0, "hint-comp-speed", NULL,  OPTION_OPTIONAL_ARGUMENT, 'f', & hints.comp_speed.multiplier},
    {0, "hint-decomp-speed", NULL,  OPTION_OPTIONAL_ARGUMENT, 'f', & hints.decomp_speed.multiplier},
    {0, "hint-lossless-range-up-to", NULL,  OPTION_OPTIONAL_ARGUMENT, 'F', & hints.lossless_data_range_up_to},
    {0, "hint-lossless-range-from", NULL,  OPTION_OPTIONAL_ARGUMENT, 'F', & hints.lossless_data_range_from},
    {0, "hint-fill-value", NULL,  OPTION_OPTIONAL_ARGUMENT, 'F', & hints.fill_value},
    {0, "hint-fake-absolute-tolerance-percent-max", "This is a fake hint. Actually it sets the abstol value based on the given percentage (enter 0.1 aka 10%% tolerance)",  OPTION_OPTIONAL_ARGUMENT, 'F', & fake_abstol_value},
    {0, "hint-fake-relative_err_finest_abs_tolerance", "This is a fake hint. Actually it sets the finest abstol value based on the given percentage (enter 0.1 aka 10%% tolerance)",  OPTION_OPTIONAL_ARGUMENT, 'F', & fake_finest_abstol_value},
    {0, "cycle", "For testing: Compress, then decompress and store the output. Files are CSV files",OPTION_FLAG, 'd' , & cycle},
    {0, "use_chunks", "For testing: use chunks",OPTION_FLAG, 'd' , & use_chunks},
    LAST_OPTION
  };

  int printhelp = 0;
  int parsed;
  SCIL_Datatype_t output_datatype;

  parsed = scilO_parseOptions(argc, argv, known_args, & printhelp);

  in_plugin = scil_find_plugin(in_file_format);
  if(! in_plugin){
    printf("Unknown format for input: %s\n", in_file_format);
    exit(1);
  }

  ret = scilO_parseOptions(argc-parsed, argv+parsed, in_plugin->get_options(), & printhelp);
  parsed += ret;

  out_plugin = scil_find_plugin(out_file_format);
  if(! out_plugin){
    printf("Unknown format for output: %s\n", out_file_format);
    exit(1);
  }
  ret = scilO_parseOptions(argc-parsed, argv+parsed, out_plugin->get_options(), & printhelp);
  if(printhelp != 0){
    printf("\nSynopsis: %s ", argv[0]);

    scilO_print_help(known_args, "-- <Input plugin options, see below> -- <Output plugin options, see below>\n");

    printf("\nPlugin options for input plugin %s\n", in_file_format);
    scilO_print_help(in_plugin->get_options(), "");

    printf("\nPlugin options for output plugin %s\n", out_file_format);
    scilO_print_help(out_plugin->get_options(), "");
    exit(0);
  }

  SCIL_Datatype_t input_datatype;
  size_t read_data_size;
  size_t array_size;

  if (use_chunks){
    int ncid, rh_id;
    scil_dims_t orig_dims;
    size_t * pos;
    size_t * count;
    scil_dims_t dims;
    size_t chunks_number=1;
    //array_size = 1*78*1440*2880*4;

    ret = in_plugin->openRead(in_file, & input_datatype, & orig_dims, & ncid, & rh_id);
    if (ret != 0){
      printf("The input file %s could not be open\n", in_file);
      exit(1);
    }
    
    pos = (size_t*) malloc(orig_dims.dims * sizeof(size_t));
    count = (size_t*) malloc(orig_dims.dims * sizeof(size_t));
    
    for (size_t i = 0; i < orig_dims.dims; i++){
      count[i] = orig_dims.length[orig_dims.dims-1-i];
      pos[i] = 0;
    }
    array_size = scil_dims_get_size(& orig_dims, input_datatype);

    chunks_number = 1;
    int i = orig_dims.dims-1;
    //split data in chunks
    while (array_size > DATA_SIZE_LIMIT){
      if ((count[i] % 2) == 0){
        count[i] >>= 1; 
	chunks_number <<= 1;
        array_size >>= 1;
      }
      else {
        if (i == 0){
          printf("Cannot be good enough chunked\n");
          exit(1);
        }
        else --i;
      }
    }
    debug("chunks: %lld, [%lld] [%lld] [%lld] [%lld]\n\n", chunks_number, count[0], count[1], count[2], count[3]);
    debug("cur_chunk: %lld | i%lld [%lld] [%lld] [%lld] [%lld]\n", 0, 0, pos[0], pos[1], pos[2], pos[3]);

    for (size_t cur_chunk = 0; cur_chunk < chunks_number; cur_chunk++){
      input_data = (byte*) scilU_safe_malloc(array_size);
      ret = in_plugin->readChunk(ncid, rh_id, input_datatype, pos, count, & input_data, & read_data_size);
      if (ret != 0){
        printf("The chunk %i could not be read\n",i);
        exit(1);
      }

      //compress
      //write

      //prepare next chunk
      for (size_t i = 0; i < orig_dims.dims; i++){
        if ((pos[i] + count[i]) < orig_dims.length[orig_dims.dims-1-i]){
           pos[i] += count[i];
           debug("cur_chunk: %lld | i%lld [%lld] [%lld] [%lld] [%lld]\n", cur_chunk+1, i, pos[0], pos[1], pos[2], pos[3]);
           break;
        }
        else {
          pos[i] = 0;
        }
      }
      free(input_data);
    }
    ret = in_plugin->closeFile(ncid);
    exit(0);
  }

  scil_timer timer;
  scil_timer totalRun;
  double t_read = 0.0, t_write = 0.0, t_compress = 0.0, t_decompress = 0.0;
  scilU_start_timer(& totalRun);
  scilU_start_timer(& timer);
  ret = in_plugin->readData(in_file, & input_data, & input_datatype, & dims, & read_data_size);
  if (ret != 0){
    printf("The input file %s could not be read\n", in_file);
    exit(1);
  }
  t_read = scilU_stop_timer(timer);

  if (use_max_value_as_fill_value){
    double max, min;
    scilU_find_minimum_maximum(input_datatype, input_data, & dims, & min, & max);
    hints.fill_value = max;
  }

  if(verbose > 0){
    double max, min;
    scilU_find_minimum_maximum_with_excluded_points(input_datatype, input_data, & dims, & min, & max, hints.lossless_data_range_up_to,  hints.lossless_data_range_from, hints.fill_value);
    printf("Min: %.10e Max: %.10e\n", min, max);
  }

  array_size = scil_dims_get_size(& dims, input_datatype);


  if (fake_abstol_value > 0.0 || fake_finest_abstol_value > 0.0){
    double max, min;
    scilU_find_minimum_maximum_with_excluded_points(input_datatype, input_data, & dims, & min, & max, hints.lossless_data_range_up_to,  hints.lossless_data_range_from, hints.fill_value);
    if (min < 0 && max < -min){
	     max = -min;
    }

    if (fake_abstol_value > 0.0){
      double new_abs_tol = max * fake_abstol_value;
      if ( hints.absolute_tolerance > 0.0 ){
        printf("Error: don't set both the absolute_tolerance and the fake relative absolute tolerance!\n");
        exit(1);
      }

      printf("fake abstol: setting value to %f (min: %f max: %f)\n", new_abs_tol, min, max);
      hints.absolute_tolerance = new_abs_tol;
    }
    if(fake_finest_abstol_value > 0.0){
      hints.relative_err_finest_abs_tolerance = max * fake_finest_abstol_value;
      printf("fake relative_err_finest_abs_tolerance: setting value to %f\n", hints.relative_err_finest_abs_tolerance);
    }
  }

  ret = scil_context_create(&ctx, input_datatype, 0, NULL, &hints);
  assert(ret == SCIL_NO_ERR);

  if (print_hints){
    printf("Effective hints (only needed for compression)\n");
    scil_user_hints_t e = scil_get_effective_hints(ctx);
    scil_user_hints_print(& e);
  }


  size_t buff_size, input_size;

  input_size = scil_get_compressed_data_size_limit(&dims, input_datatype);
  output_data = (byte*) scilU_safe_malloc(input_size);

  if (cycle || (! compress && ! uncompress) ){
    printf("...compression and decompression\n");
    byte* result = (byte*) scilU_safe_malloc(input_size);

    scilU_start_timer(& timer);
    ret = scil_compress(result, input_size, input_data, & dims, & buff_size, ctx);
    t_compress = scilU_stop_timer(timer);
    assert(ret == SCIL_NO_ERR);

    byte* tmp_buff = (byte*) scilU_safe_malloc(input_size);
    scilU_start_timer(& timer);
    ret = scil_decompress(input_datatype, output_data, & dims, result, buff_size, tmp_buff);
    t_decompress = scilU_stop_timer(timer);
    assert(ret == SCIL_NO_ERR);

    free(tmp_buff);

    output_datatype = input_datatype;

    if (validate) {
        ret = scil_validate_compression(input_datatype, input_data, &dims, result, buff_size, ctx, &out_accuracy);
        if(ret != SCIL_NO_ERR){
          printf("SCIL validation error!\n");
        }
        if(print_hints){
          printf("Validation accuracy:");
          scil_user_hints_print(& out_accuracy);
        }
    }
    ret = scil_destroy_context(ctx);
    assert(ret == SCIL_NO_ERR);

  } else if (compress){
    printf("...compression\n");
    scilU_start_timer(& timer);
    ret = scil_compress(output_data, input_size, input_data, & dims, & buff_size, ctx);
    t_compress = scilU_stop_timer(timer);
    assert(ret == SCIL_NO_ERR);

    if (validate) {
        ret = scil_validate_compression(input_datatype, input_data, &dims, output_data, buff_size, ctx, &out_accuracy);
        if(ret != SCIL_NO_ERR){
          printf("SCIL validation error!\n");
        }
        if(print_hints){
          printf("Validation accuracy:");
          scil_user_hints_print(& out_accuracy);
        }
    }
    ret = scil_destroy_context(ctx);
    assert(ret == SCIL_NO_ERR);

    output_datatype = SCIL_TYPE_BINARY;
  } else if (uncompress){
    printf("...decompression\n");
    byte* tmp_buff = (byte*) scilU_safe_malloc(input_size);
    scilU_start_timer(& timer);
    ret = scil_decompress(input_datatype, output_data, & dims, input_data, read_data_size, tmp_buff);
    t_decompress = scilU_stop_timer(timer);
    free(tmp_buff);
    assert(ret == SCIL_NO_ERR);

    output_datatype = input_datatype;
  }

  // todo reformat into output format, if neccessary
  if (out_file != NULL){
    if ( compute_residual && (uncompress || cycle) ){
      // compute the residual error
      scilU_subtract_data(input_datatype, input_data, output_data, & dims);
    }

    scilU_start_timer(& timer);
    ret = out_plugin->writeData(out_file, output_data, output_datatype, buff_size, input_datatype, dims);
    t_write = scilU_stop_timer(timer);
    if (ret != 0){
      printf("The output file %s could not be written\n", out_file);
      exit(1);
    }
  }
	double runtime = scilU_stop_timer(totalRun);
  if(measure_time){
    printf("Size:\n");
    printf(" size, %ld\n size_compressed, %ld\n ratio, %f\n", array_size, buff_size, ((double) buff_size) / array_size);
    printf("Runtime:  %fs\n", runtime);
    printf(" read,       %fs, %f MiB/s\n", t_read, array_size/t_read/1024 /1024);
    if (t_compress > 0.0)
      printf(" compress,   %fs, %f MiB/s\n", t_compress, array_size/t_compress/1024 /1024);
    if (t_decompress > 0.0)
      printf(" decompress, %fs, %f MiB/s\n", t_decompress, array_size/t_decompress/1024 /1024);
    if (t_write > 0.0)
      printf(" write,      %fs, %f MiB/s\n", t_write, array_size/t_write/1024 /1024);
  }

  free(input_data);
  free(output_data);

  return 0;
}
