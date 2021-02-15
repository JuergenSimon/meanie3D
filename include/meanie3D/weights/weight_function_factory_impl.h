/* The MIT License (MIT)
 * 
 * (c) Jürgen Simon 2014 (juergen.simon@uni-bonn.de)
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef M3D_WEIGHT_FUNCTION_FACTORY_IMPL_H
#define    M3D_WEIGHT_FUNCTION_FACTORY_IMPL_H

#include "weight_function.h"
#include "oase_weights.h"
#include "ci_weights.h"
#include "default_weights.h"
#include "inverse_default.h"
#include "exp10_weight.h"
#include "inv_erfc_weight.h"
#include "no_weight.h"

#include "weight_function_factory.h"

#include <meanie3D/clustering/detection.h>

namespace m3D {

    template<typename T>
    WeightFunction <T> *
    WeightFunctionFactory<T>::create(const detection_params_t <T> &params,
                                     const detection_context_t <T> &ctx) {
        if (params.verbosity > VerbositySilent) {
            cout << endl << "Constructing " << params.weight_function_name << " weight function ...";
            start_timer();
        }

        WeightFunction<T> *weight_function = NULL;

        if (params.weight_function_name == "oase-ci") 
        {
            weight_function = new OASECIWeightFunction<T>(params, ctx);
        } 
        else if (params.weight_function_name == "oase") 
        {
            weight_function = new OASEWeightFunction<T>(params, ctx);
        } 
        else if (params.weight_function_name == "inverse") 
        {
            weight_function = new InverseDefaultWeightFunction<T>(params, ctx);
        } 
        else if (params.weight_function_name == "pow10") {
            weight_function = new EXP10WeightFunction<T>(params, ctx);
        }
        else if (params.weight_function_name == "inverfc")
        {
            weight_function = new InvErfcWeightFunction<T>(params, ctx);
        }
        else if (params.weight_function_name == "none") 
        {
            weight_function = new NoWeightFunction<T>();
        }
        else
        {
            weight_function = new DefaultWeightFunction<T>(params, ctx);
        }

        return weight_function;
    }
}

#endif	/* M3D_WEIGHT_FUNCTION_FACTORY_IMPL_H */

