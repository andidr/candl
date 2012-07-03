
   /**------ ( ----------------------------------------------------------**
    **       )\                      CAnDL                               **
    **----- /  ) --------------------------------------------------------**
    **     ( * (                    candl.c                              **
    **----  \#/  --------------------------------------------------------**
    **    .-"#'-.        First version: september 8th 2003               **
    **--- |"-.-"| -------------------------------------------------------**
    |     |
    |     |
 ******** |     | *************************************************************
 * CAnDL  '-._,-' the Chunky Analyzer for Dependences in Loops (experimental) *
 ******************************************************************************
 *                                                                            *
 * Copyright (C) 2003-2008 Cedric Bastoul                                     *
 *                                                                            *
 * This is free software; you can redistribute it and/or modify it under the  *
 * terms of the GNU General Public License as published by the Free Software  *
 * Foundation; either version 2 of the License, or (at your option) any later *
 * version.                                                                   *
 *                                                                            *
 * This software is distributed in the hope that it will be useful, but       *
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY *
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License   *
 * for more details.                                                          *
 *                                                                            *
 * You should have received a copy of the GNU General Public License along    *
 * with software; if not, write to the Free Software Foundation, Inc.,        *
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA                     *
 *                                                                            *
 * CAnDL, the Chunky Dependence Analyser                                      *
 * Written by Cedric Bastoul, Cedric.Bastoul@inria.fr                         *
 *                                                                            *
 ******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <osl/scop.h>
#include <osl/macros.h>
#include <osl/extensions/dependence.h>
#include <osl/util.h>
#include <candl/candl.h>
#include <candl/dependence.h>
#include <candl/violation.h>
#include <candl/options.h>
#include <candl/usr.h>
#include <candl/util.h>


int main(int argc, char * argv[]) {
  
  osl_scop_p scop = NULL;
  osl_scop_p orig_scop = NULL;
  osl_dependence_p orig_dependence = NULL;
  candl_options_p options;
  candl_violation_p violation = NULL;
  FILE *input, *output, *input_test;
  int precision;
  #if defined(LINEAR_VALUE_IS_INT)
    precision = OSL_PRECISION_SP;
  #elif defined(LINEAR_VALUE_IS_LONGLONG)
    precision = OSL_PRECISION_DP;
  #elif defined(LINEAR_VALUE_IS_MP)
    precision = OSL_PRECISION_MP;
  #endif

  /* Options and input/output file setting. */
  candl_options_read(argc, argv, &input, &output, &input_test, &options);
  
  /* Open the scop
   * If there is no original scop (given with the -test), the input file
   * is considered as the original scop
   */
  osl_interface_p registry = osl_interface_get_default_registry();
  if (input_test != NULL) {
    scop = osl_scop_pread(input, registry, precision);
    orig_scop = osl_scop_pread(input_test, registry, precision);
  } else {
    orig_scop = osl_scop_pread(input, registry, precision);
  }
  osl_interface_free(registry);
  
  if (orig_scop == NULL || (scop == NULL && input_test != NULL)) {
    CANDL_error("Fail to open scop");
    exit(1);
  }
  
  /* Check if we can compare the two scop
   * The function compare only domains and access arrays
   */
  if (input_test != NULL && !candl_util_check_scop(orig_scop, scop))
    CANDL_error("The two scop can't be compared");
  
  /* Add more infos (depth, label, ...)
   * Not important for the transformed scop
   * TODO : the statements must be sorted to compute the statement label
   *        the problem is if the scop is reordered, the second transformed scop
   *        must be aligned with it
   */
  candl_usr_init(orig_scop);
  
  /* Calculating dependences. */
  orig_dependence = osl_generic_lookup(orig_scop->extension, OSL_URI_DEPENDENCE);
  if (orig_dependence != NULL) {
    CANDL_info("Dependences read from the optional tag");
  } else {
    orig_dependence = candl_dependence(orig_scop, options);
    osl_generic_p data = osl_generic_shell(orig_dependence,
                                           osl_dependence_interface());
    data->next = orig_scop->extension;
    orig_scop->extension = data;
  }

  /* Calculating legality violations. */
  if (input_test != NULL)
    violation = candl_violation(orig_scop, orig_dependence, scop, options);

  /* Printing data structures if asked. */
  if (options->structure) {
    if (input_test != NULL) {
      fprintf(output, "\033[33mORIGINAL SCOP:\033[00m\n");
      osl_scop_print(output, orig_scop);
      fprintf(output, "\n\033[33mTRANSFORMED SCOP:\033[00m\n");
      osl_scop_print(output, scop);
      fprintf(output, "\n\033[33mDEPENDENCES GRAPH:\033[00m\n");
      candl_dependence_pprint(output, orig_dependence);
      fprintf(output, "\n\n\033[33mVIOLATIONS GRAPH:\033[00m\n");
      candl_violation_pprint(output, violation);
    } else {
      fprintf(output, "\033[33mORIGINAL SCOP:\033[00m\n");
      osl_scop_print(output, orig_scop);
      fprintf(output, "\n\033[33mDEPENDENCES GRAPH:\033[00m\n");
      candl_dependence_pprint(output, orig_dependence);
    }
  } else if (input_test != NULL) {
     /* Printing violation graph */
    candl_violation_pprint(output, violation);
    if (options->view)
      candl_violation_view(violation);
  } else if (options->outscop) {
    /* Export to the scop format */
    osl_scop_print(output, orig_scop);
  } else {
    /* Printing dependence graph if asked or if there is no transformation. */
    candl_dependence_pprint(output, orig_dependence);
    if (options->view)
      candl_dependence_view(orig_dependence);
  }

  /* Being clean. */
  if (input_test != NULL) {
    candl_violation_free(violation);
    osl_scop_free(scop);
    fclose(input_test);
  }
  
  // the dependence is freed with the scop
  //osl_dependence_free(orig_dependence); 
  candl_options_free(options);
  candl_usr_cleanup(orig_scop);
  osl_scop_free(orig_scop);
  fclose(input);
  fclose(output);
  pip_close();
  
  return 0;
}
