/* 
 * File:   detection_commandline.h
 * Author: simon
 *
 * Created on January 25, 2016, 3:40 PM
 */

#ifndef M3D_DETECTION_COMMANDLINE_H
#define M3D_DETECTION_COMMANDLINE_H

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>
#include <meanie3D/filters/replacement_filter.h>

#include <boost/tokenizer.hpp>
#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/exception/info.hpp>

#include <map>
#include <netcdf>
#include <string>
#include <vector>
#include <set>

#include "detection.h"

namespace m3D
{

    /**
     * Add the command line description for the detection related paramaters.
     * 
     * @param desc
     * @param params
     */
    template <typename T>
    void
    add_detection_options(program_options::options_description &desc,
                          const detection_params_t<T> &params)
    {
        desc.add_options()
        ("file,f", program_options::value<string>(), "CF-Metadata compliant NetCDF-file")
        ("output,o", program_options::value<string>(), "Name of output file for clustering results")
        ("dimensions,d", program_options::value<string>(), "Comma-separated list of the dimensions to be used. The program expects dimension variables with identical names.")
        ("variables,v",program_options::value<string>(),"Comma-separated variables used to construct feature space. Do not include dimension variables")
        ("time-index,t",program_options::value<int>()->default_value(params.time_index), "Index of the point in time you wish to use in files with a time dimension. -1 means no time dimension in the variables.")
        ("lower-thresholds",program_options::value<string>(),"Comma-separated list var1=val,var2=val,... of lower thresholds. Values below this are ignored when constructing feature space")
        ("upper-thresholds", program_options::value<string>(), "Comma-separated list var1=val,var2=val,... of lower thresholds. Values above this are ignored when constructing feature space")
        ("replacement-values", program_options::value<string>(),"Comma-separated list var1=val,var2=val,... of values to replace missing values with in feature space construction. If no replacement value is specified while even one variable is out of valid range at one point, the whole point is discarded")
        ("replacement-filter", program_options::value<string>(), "Comma-separated list varname-<lowest|highest|median>-[percentage], to replace values with average of lowest/highest percent or median of neighbours")
        ("kernel-name,k", program_options::value<string>()->default_value(params.kernel_name), "uniform,gauss,epanechnikov or none")
        #if WITH_SATELLITE
        ("weight-function-name,w", program_options::value<string>()->default_value(params.weight_function_name), "none, default, inverse, pow10, inverfc, oase or oaseci")
        #else 
        ("weight-function-name,w", program_options::value<string>()->default_value(params.weight_function_name), "none, default, inverse, pow10 or inverfc")
        #endif
        ("wwf-lower-threshold",program_options::value<T>()->default_value(params.wwf_lower_threshold), "Lower threshold for weight function filter.")
        ("wwf-upper-threshold", program_options::value<T>()->default_value(params.wwf_upper_threshold), "Upper threshold for weight function filter.")
        ("scale,s", program_options::value<double>()->default_value(params.scale), "Scale parameter to pre-smooth the data with. Filter size is calculated from this automatically.")
        ("filter-size,l", program_options::value<double>()->default_value(0.0), "Scale parameter to pre-smooth the data with. Scale parameter is calculated from this automatically.")
        ("ranges,r", program_options::value<string>(), "Override the automatic bandwidth calculation with a set of given bandwidths. Use in the order of (dim1,...dimN,var1,...,varN).")
#if WITH_SATELLITE
        ("ci-comparison-file", program_options::value<string>(), "File for calculating time trends for CI-score according to Walker et al. 2012.")
        ("ci-comparison-protocluster-file", program_options::value<string>(), "Protoclusters from the comparison file")
        ("ci-satellite-only", "If present, only satellite values are used (original score), otherwise ")
        ("ci-use-walker-mecikalski","used for CI score. If absent, the modified version is used")
        ("ci-protocluster-scale", program_options::value<T>()->default_value(params.ci_protocluster_scale), "Scale parameter for protocluster detection")
        ("ci-protocluster-min-size", program_options::value<int>()->default_value(params.ci_protocluster_min_size), "Minimum size of protoclusters")
#endif
        ("inline-tracking", "If present, tracking step is performed immediately after clustering. Required --previous-output and other clustering parameters to be set (check meanie3D-track)")
        ("coalesce-with-strongest-neighbour", "If present, clusters are post-processed, coalescing each cluster their strongest neighbour")
        ("postprocess-with-previous-output", "If present, the --previous-output file is used to consolidate current results. This is time consuming and has a propensity to form larger clusters")
        ("previous-output,p", program_options::value<string>(), "Previous cluster result file. Used for tracking or to keep the clustering more stable over time (--postprocess-with-previous-output)")
        ("previous-cluster-coverage-threshold", program_options::value<double>()->default_value(params.cluster_coverage_threshold), "Minimum overlap in percent between current and previous clusters to be taken into consideration.")
        ("min-cluster-size,m", program_options::value<unsigned int>()->default_value(params.min_cluster_size), "Discard clusters smaller than this number of points.")
        ("include-weight-function-in-results,i", "Add a netcdf variable 'weight' to the result file, containing the weight function response at each point in the feature-space")
#if WITH_VTK
        ("write-variables-as-vtk", program_options::value<string>(), "Comma separated list of variables that should be written out as VTK files (after applying scale/threshold)")
        ("write-weight-function", "Write weight function out as .vtk file")
        ("write-meanshift-vectors", "Write out .vtk files containing the meanshift vectors")
        ("write-clusters-as-vtk", "Write clusters out as .vtk files")
        ("write-cluster-modes", "Write the final meanshift modes in .vtk file format")
        ("write-cluster-centers","Write cluster centers out in .vtk file format")
        ("write-cluster-weight-response", "Write out the clusters with weight responses as value")
#endif
        ;
    }

    template <typename T>
    void get_detection_parameters(program_options::variables_map vm,
                                  detection_params_t<T> &params)
    {
        if (vm.count("file") == 0)
        {
            cerr << "Missing input file argument" << endl;
            exit(EXIT_FAILURE);
        }
        params.filename = vm["file"].as<string>();
        try
        {
            params.output_filename = vm["output"].as<string>();
        }
        catch (const boost::exception &e)
        {
            cerr << "Missing parameter -o " << endl;
            exit(EXIT_FAILURE);
        }

        // Open NetCDF file
        NcFile *file = NULL;
        try
        {
            file = new NcFile(params.filename, NcFile::read);
        }
        catch (const netCDF::exceptions::NcException &e)
        {
            cerr << "Error opening file '" << params.filename << "' : " << e.what() << endl;
            exit(EXIT_FAILURE);
        }

        // Extract dimensions
        if (vm.count("dimensions") == 0)
        {
            cerr << "Missing parameter --dimensions" << endl;
            exit(EXIT_FAILURE);
        }

        // parse dimension list
        typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
        boost::char_separator<char> sep(",");
        tokenizer dim_tokens(vm["dimensions"].as<string>(), sep);
        for (tokenizer::iterator tok_iter = dim_tokens.begin(); tok_iter != dim_tokens.end(); ++tok_iter)
        {
            const char *name = (*tok_iter).c_str();
            params.dimensions.push_back(name);
            NcVar dimVar = file->getVar(name);
            if (dimVar.isNull())
            {
                cerr << "No dimension variable '" << std::string(name) << "' exists in file " << params.filename << endl;
                exit(EXIT_FAILURE);
            }
            params.dimension_variables.push_back(name);
        }
        params.parameters = params.parameters + "dimensions=" + vm["dimensions"].as<string>() + " ";

        // parse variables
        if (vm.count("variables") == 0)
        {
            cerr << "Missing mandatory parameter --variables" << endl;
            exit(EXIT_FAILURE);
            ;
        }
        tokenizer var_tokens(vm["variables"].as<string>(), sep);
        for (tokenizer::iterator tok_iter = var_tokens.begin(); tok_iter != var_tokens.end(); ++tok_iter)
        {
            std::string name = *tok_iter;
            NcVar var = file->getVar(name);
            if (var.isNull())
            {
                cerr << "No variable '" << std::string(*tok_iter) << "' exists!" << endl;
                exit(EXIT_FAILURE);
                ;
            }
            params.variables.push_back(name);
        }
        params.parameters += params.parameters + "variables=" + vm["variables"].as<string>() + " ";

        // time
        params.time_index = vm["time-index"].as<int>();

        // Convection filter index
        params.convection_filter_index = -1;
        if (vm.count("convection-filter-variable") > 0)
        {
            std::string cf_var_name = vm["convection-filter-variable"].as<string>();
            for (int index = 0; index < params.variables.size(); index++)
            {
                std::string name = params.variables.at(index);
                if (name == cf_var_name)
                {
                    // give the index in the complete feature-space vector
                    // not just the list of variables handed in
                    params.convection_filter_index = params.dimensions.size() + index;
                }
            }

            if (params.convection_filter_index < 0)
            {
                cerr << "Bad value for convection-filter-variable. Variable '"
                     << cf_var_name << "' is not a feature space variable"
                     << endl;
                exit(EXIT_FAILURE);
            }
        }

        // replacement filter
        if (vm.count("replacement-filter") > 0)
        {
            // format: variableName-replacement-mode[-percentage]
            // RX-median
            // RX-lowest-25
            // RX-highest-50
            std::string rf = vm["replacement-filter"].as<string>();
            tokenizer tokens(rf, sep);
            for (tokenizer::iterator ti = tokens.begin(); ti != tokens.end(); ++ti)
            {
                tokenizer filter_tokens(*ti, boost::char_separator<char>("-"));
                size_t i = 0;
                tokenizer::iterator fi = filter_tokens.begin();
                typename ReplacementFilter<T>::ReplacementMode mode;
                int variable_index = -1;
                float percentage = -1.0;
                while (fi != filter_tokens.end())
                {
                    if (i == 0)
                    {
                        std::string varName = *fi;
                        for (size_t vi = 0; vi < params.variables.size(); vi++)
                        {
                            if (params.variables[vi] == varName)
                            {
                                variable_index = vi;
                                break;
                            }
                        }
                        if (variable_index < 0)
                        {
                            cerr << "Illegal variable name for --replacement-filter" << endl;
                            exit(EXIT_FAILURE);
                            ;
                        }
                    }
                    else if (i == 1)
                    {
                        std::string modeName = *fi;
                        if (modeName == "median")
                        {
                            mode = ReplacementFilter<T>::ReplaceWithMedian;
                        }
                        else if (modeName == "lowest")
                        {
                            mode = ReplacementFilter<T>::ReplaceWithLowest;
                        }
                        else if (modeName == "highest")
                        {
                            mode = ReplacementFilter<T>::ReplaceWithHighest;
                        }
                        else
                        {
                            cerr << "Illegal mode name for --replacement-filter" << endl;
                            exit(EXIT_FAILURE);
                            ;
                        }
                    }
                    else if (i == 2)
                    {
                        std::string perc = *fi;
                        percentage = strtof(perc.c_str(), (char **)NULL) / 100.0f;
                    }
                    i++;
                    fi++;
                }

                if ((mode == ReplacementFilter<T>::ReplaceWithHighest || mode == ReplacementFilter<T>::ReplaceWithLowest) && percentage < 0)
                {
                    cerr << "Missing percentage in --replacement-filter" << endl;
                    exit(EXIT_FAILURE);
                    ;
                }

                params.replacementFilterVariableIndex.push_back(variable_index);
                params.replacementFilterPercentages[variable_index] = percentage;
                params.replacementFilterModes[variable_index] = mode;
            }
        }

        // parse ranges if there
        if (vm.count("ranges") > 0)
        {
            tokenizer bw_tokens(vm["ranges"].as<string>(), sep);
            for (tokenizer::iterator tok_iter = bw_tokens.begin(); tok_iter != bw_tokens.end(); ++tok_iter)
            {
                const char *bw = (*tok_iter).c_str();
                params.ranges.push_back((T)strtod(bw, (char **)NULL));
            }
            if (params.ranges.size() != params.dimension_variables.size() + params.variables.size())
            {
                cerr << "Please provide " << params.dimension_variables.size() + params.variables.size() << " bandwidth values" << endl;
                exit(EXIT_FAILURE);
            }
            params.parameters = params.parameters + "ranges=" + vm["ranges"].as<string>();
        }

        // Lower Thresholds

        if (vm.count("lower-thresholds") > 0)
        {
            boost::char_separator<char> equals("=");

            tokenizer tokens(vm["lower-thresholds"].as<string>(), sep);

            for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
            {
                std::string pair = *tok_iter;
                tokenizer subtokens(pair, equals);
                tokenizer::iterator subtoken_iter = subtokens.begin();
                std::string variableName = *subtoken_iter;
                bool have_var = false;
                for (int i = 0; i < params.variables.size() && !have_var; i++)
                {
                    if (params.variables[i] == variableName)
                    {
                        subtoken_iter++;
                        if (subtoken_iter == subtokens.end())
                        {
                            cerr << "Missing threshold value for variable " << variableName << endl;
                            exit(EXIT_FAILURE);
                        }
                        const char *value = (*subtoken_iter).c_str();
                        params.lower_thresholds[i] = boost::numeric_cast<T>(strtod(value, (char **)NULL));
                        have_var = true;
                    }
                }

                if (!have_var)
                {
                    cerr << "No variable named " << variableName << " found. Check --lower-thresholds parameter"
                         << endl;
                    exit(EXIT_FAILURE);
                }
            }
        }

        // Upper Thresholds
        if (vm.count("upper-thresholds") > 0)
        {
            boost::char_separator<char> equals("=");
            tokenizer tokens(vm["upper-thresholds"].as<string>(), sep);
            for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
            {
                std::string pair = *tok_iter;
                tokenizer subtokens(pair, equals);
                tokenizer::iterator subtoken_iter = subtokens.begin();
                std::string variableName = *subtoken_iter;
                bool have_var = false;
                for (int i = 0; i < params.variables.size(); i++)
                {
                    if (params.variables[i] == variableName)
                    {
                        subtoken_iter++;
                        if (subtoken_iter == subtokens.end())
                        {
                            cerr << "Missing threshold value for variable " << variableName << endl;
                            exit(EXIT_FAILURE);
                        }
                        const char *value = (*subtoken_iter).c_str();
                        params.upper_thresholds[i] = boost::numeric_cast<T>(strtod(value, (char **)NULL));
                        have_var = true;
                    }
                }
                if (!have_var)
                {
                    cerr << "No variable named " << variableName << " found. Check --upper-thresholds parameter"
                         << endl;
                    exit(EXIT_FAILURE);
                }
            }
        }

        // Fill values
        if (vm.count("replacement-values") > 0)
        {
            boost::char_separator<char> equals("=");
            tokenizer tokens(vm["replacement-values"].as<string>(), sep);
            for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter)
            {
                std::string pair = *tok_iter;
                tokenizer subtokens(pair, equals);
                tokenizer::iterator subtoken_iter = subtokens.begin();
                std::string variableName = *subtoken_iter;

                // TODO: utilizes netCDF specific feature (getId()) which
                // should be removed to decouple this from netCDF layer.

                NcVar variable;
                for (size_t i = 0; i < params.variables.size(); i++)
                {
                    if (params.variables[i] == variableName)
                    {
                        variable = file->getVar(params.variables[i]);
                    }
                }
                if (variable.isNull())
                {
                    cerr << "No variable named " << variableName << " found. Check --replacement-values parameter"
                         << endl;
                    exit(EXIT_FAILURE);
                }
                subtoken_iter++;
                if (subtoken_iter == subtokens.end())
                {
                    cerr << "Missing replacement value for variable " << variableName << endl;
                    exit(EXIT_FAILURE);
                }
                const char *value = (*subtoken_iter).c_str();
                double doubleValue = strtod(value, (char **)NULL);
                params.replacement_values[variable.getId()] = doubleValue;
            }
        }

        // Scale parameter
        params.scale = vm["scale"].as<double>();

        // Kernel
        params.kernel_name = vm["kernel-name"].as<string>();
        std::set<std::string> validKernels = {"uniform", "epanechnikov", "gauss", "none"};
        if (validKernels.find(params.kernel_name) == validKernels.end())
        {
            cerr << "Illegal kernel name " << params.kernel_name << ". Use one of " << validKernels << endl;
            exit(EXIT_FAILURE);
        }

        // Weight Function
        params.weight_function_name = vm["weight-function-name"].as<string>();
        std:set<std::string> validWeights = {"none", "default", "inverse", "pow10", "inverfc"};
        #if WITH_SATELLITE
        validWeights.insert("oase");
        validWeights.insert("oase-ci");
        #endif

        if (validWeights.find(params.weight_function_name) == validWeights.end())
        {
            cerr << "Illegal weight function name " << params.weight_function_name << ". Use one of " << validWeights << endl;
            exit(EXIT_FAILURE);
        }
        params.wwf_lower_threshold = vm["wwf-lower-threshold"].as<T>();
        params.wwf_upper_threshold = vm["wwf-upper-threshold"].as<T>();

        // Coalescence?
        params.coalesceWithStrongestNeighbour = vm.count("coalesce-with-strongest-neighbour") > 0;

        // only spatial range?
        params.spatial_range_only = vm.count("spatial-range-only") > 0;

#if WITH_VTK
        // VTK output?
        params.write_vtk = vm.count("write-clusters-as-vtk") > 0;
        params.write_cluster_modes = vm.count("write-cluster-modes") > 0;
        params.write_cluster_centers = vm.count("write-cluster-centers") > 0;
        params.write_weight_response = vm.count("write-cluster-weight-response") > 0;
        params.write_weight_function = vm.count("write-weight-function") > 0;
        params.write_meanshift_vectors = vm.count("write-meanshift-vectors") > 0;
#endif

        // include weight function output in result?
        params.include_weight_in_result = vm.count("include-weight-function-in-results") > 0;

        // Previous file
        if (vm.count("previous-output") > 0)
        {
            std::string previous = vm["previous-output"].as<string>();
            boost::filesystem::path previous_path(previous);
            if (boost::filesystem::exists(previous_path) && boost::filesystem::is_regular_file(previous_path))
            {
                params.previous_clusters_filename = new std::string(previous);
            }
            else
            {
                cerr << "Illegal value for parameter --previous-output:"
                     << "does not exist or is no regular file" << endl;
                exit(EXIT_FAILURE);
            }
        }

        // Use previous output to consolidate?
        params.postprocess_with_previous_output = vm.count("postprocess-with-previous-output") > 0;

        // CI flags
#if WITH_SATELLITE
        params.ci_satellite_only = vm.count("ci-satellite-only") > 0;
        params.ci_use_walker_mecikalski = vm.count("ci-use-walker-mecikalski") > 0;

        // ci_comparison_file
        if (vm.count("ci-comparison-file") > 0)
        {
            std::string previous = vm["ci-comparison-file"].as<string>();
            boost::filesystem::path previous_path(previous);
            if (boost::filesystem::exists(previous_path) && boost::filesystem::is_regular_file(previous_path))
            {
                params.ci_comparison_file = new std::string(previous);
            }
            else
            {
                cerr << "FATAL:illegal value for parameter --ci-comparison-file"
                     << ": does not exist or is no regular file" << endl;
                exit(EXIT_FAILURE);
            }
        }

        // ci-comparison-protocluster-file
        if (vm.count("ci-comparison-protocluster-file") > 0)
        {
            std::string previous = vm["ci-comparison-protocluster-file"].as<string>();
            boost::filesystem::path previous_path(previous);
            if (boost::filesystem::exists(previous_path) && boost::filesystem::is_regular_file(previous_path))
            {
                params.ci_comparison_protocluster_file = new std::string(previous);
            }
            else
            {
                cerr << "FATAL:illegal value for parameter --ci-comparison-protocluster-file: "
                     << "does not exist or is no regular file" << endl;
                exit(EXIT_FAILURE);
            }
        }

        if (vm.count("ci-protocluster-scale") > 0)
        {
            params.ci_protocluster_scale = vm["ci-protocluster-scale"].as<T>();
        }
        if (vm.count("ci-protocluster-min-size") > 0)
        {
            params.ci_protocluster_min_size = vm["ci-protocluster-min-size"].as<int>();
        }
#endif

        // previous-cluster-coverage-threshold
        params.cluster_coverage_threshold = vm["previous-cluster-coverage-threshold"].as<T>();

        // min cluster size
        params.min_cluster_size = vm["min-cluster-size"].as<unsigned int>();

        // Inline tracking?
        params.inline_tracking = vm.count("inline-tracking") > 0;

        if (params.inline_tracking && params.previous_clusters_filename == NULL)
        {
            cerr << "FATAL:when --inline-tracking is set you must give "
                 << "--previous-output as well" << endl;
            exit(EXIT_FAILURE);
        }

        // vtk-variables
#if WITH_VTK
        if (vm.count("write-variables-as-vtk") > 0)
        {
            tokenizer bw_tokens(vm["write-variables-as-vtk"].as<string>(), sep);
            for (tokenizer::iterator tok_iter = bw_tokens.begin(); tok_iter != bw_tokens.end(); ++tok_iter)
            {
                std::string name = *tok_iter;
                try
                {
                    NcVar var = file->getVar(name);
                    if (var.isNull())
                    {
                        cerr << "Can't open variable " << name
                             << " from NetCDF file. Check --write-variables-as-vtk"
                             << endl;
                        exit(EXIT_FAILURE);
                    }
                    params.vtk_variables.push_back(name);
                }
                catch (const netCDF::exceptions::NcException &e)
                {
                    cerr << "Can't find variable " << name
                         << " from NetCDF file. Check --write-variables-as-vtk"
                         << endl;
                    exit(EXIT_FAILURE);
                }
            }
        }
#endif
    }

    /**
     * Print feedback on the given command line options.
     * 
     * @param params
     * @param ctx pre-initialised detection context
     * @param vm
     */
    template <typename T>
    void print_detection_params(const detection_params_t<T> &params,
                                const detection_context_t<T> &ctx,
                                const program_options::variables_map &vm)
    {
        cout << "\tinput file = " << params.filename << endl;
        if (params.time_index >= 0)
        {
            cout << "\tusing point in time at index " << params.time_index
                 << " (timestamp=" << ctx.timestamp << ")" << endl;
        }
        else
        {
            cout << "\ttime is not a variable dimension. Using timestamp "
                 << ctx.timestamp << endl;
        }
        cout << "\tdimensions = " << vm["dimensions"].as<string>() << endl;
        cout << "\tvariables = " << vm["variables"].as<string>() << endl;

        if (!params.ranges.empty())
        {
            cout << "\tranges = " << params.ranges << endl;
        }
        else
        {
            cout << "\tautomatic bandwidth selection" << endl;
        }

        if (!params.lower_thresholds.empty())
        {
            cout << "\tusing lower thresholds " << vm["lower-thresholds"].as<string>() << endl;
        }

        if (!params.upper_thresholds.empty())
        {
            cout << "\tusing upper thresholds " << vm["upper-thresholds"].as<string>() << endl;
        }

        if (params.scale != Detection<T>::NO_SCALE)
        {
            cout << "\tpre-smoothing data with scale parameter "
                 << params.scale << " (kernel width = "
                 << ctx.kernel_width << ")" << endl;
        }
        else
        {
            cout << "\tno scale-space smoothing" << endl;
        }

        cout << "\tkernel:" << params.kernel_name << endl;
        cout << "\tweight-function:" << params.weight_function_name << endl;
        cout << "\t\tlower weight-function threshold: "
             << params.wwf_lower_threshold << endl;
        cout << "\t\tupper weight-function threshold: "
             << params.wwf_upper_threshold << endl;

        if (params.ci_comparison_file != NULL)
        {
            cout << "\tCI comparison file for time trends:"
                 << *params.ci_comparison_file << endl;
        }

        if (params.previous_clusters_filename != NULL)
        {
            cout << "\tprevious file:" << *params.previous_clusters_filename << endl;
        }

        if (params.postprocess_with_previous_output)
        {
            if (vm.count("previous-cluster-coverage-threshold") > 0)
            {
                cout << "\tprevious file coverage threshold:"
                     << params.cluster_coverage_threshold << endl;
            }
        }

        if (params.min_cluster_size > 1)
        {
            cout << "\tminimum cluster size = "
                 << params.min_cluster_size << endl;
        }

        if (!params.replacementFilterVariableIndex.empty())
        {
            cout << "\treplacement filters on " << endl;
            for (size_t i = 0; i < params.replacementFilterVariableIndex.size(); i++)
            {
                int rvi = params.replacementFilterVariableIndex[i];
                cout << "\t\t" << params.variables[rvi] << " replace with ";
                typename ReplacementFilter<T>::ReplacementMode mode = params.replacementFilterModes.at(rvi);
                float percent = params.replacementFilterPercentages.at(rvi) * 100.0;
                switch (mode)
                {
                case ReplacementFilter<T>::ReplaceWithHighest:
                    cout << "highest " << percent << "%";
                    break;
                case ReplacementFilter<T>::ReplaceWithLowest:
                    cout << "lowest " << percent << "%";
                    break;
                case ReplacementFilter<T>::ReplaceWithMedian:
                    cout << "median";
                    break;
                }
                cout << " of neighboring points" << endl;
            }
        }

        cout << "\tmean-shift is limited to spatial range: "
             << (params.spatial_range_only ? "yes" : "no") << endl;

        cout << "\tcoalesce results with strongest neighbor: "
             << (params.coalesceWithStrongestNeighbour ? "yes" : "no") << endl;

        cout << "\toutput written to file: " << params.output_filename << endl;

#if WITH_VTK
        if (!params.vtk_variables.empty())
        {
            cout << "\twriting out these variables as vtk after processing:" << vm["write-variables-as-vtk"].as<string>() << endl;
        }
        cout << "\twriting weight function to vtk:" << (params.write_weight_function ? "yes" : "no") << endl;
        cout << "\twriting mean-shift vectors to vtk:" << (params.write_meanshift_vectors ? "yes" : "no") << endl;
        cout << "\tclusters written as vtk: " << (params.write_vtk ? "yes" : "no") << endl;
        cout << "\twrite cluster centers as vtk: " << (params.write_cluster_centers ? "yes" : "no") << endl;
        cout << "\twriting cluster weights as vtk: " << (params.write_weight_response ? "yes" : "no") << endl;
#endif
    }
} // namespace m3D

#endif /* M3D_DETECTION_COMMANDLINE_H */
