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

#ifndef M3D_INVERFCWEIGHTFUNCTION_H
#define M3D_INVERFCWEIGHTFUNCTION_H

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>
#include <meanie3D/utils.h>
#include <meanie3D/weights/weight_function.h>
#include <meanie3D/clustering/detection.h>

#include <netcdf>
#include <vector>
#include <map>

namespace m3D {

    /** 
     * This weight function scales from [0 .. 1] following the inverse
     * complementary gaussian error function:
     * 
     *              f(x) = 1-erfc(2x)
     * 
     * The curve has a strong effect in the beginning and levels out 
     * quickly at 1. This weight function can be used to help sharpen
     * the boundaries in objects that are in close proximity. 
     * 
     * The weight is calculated by iterating over all variables. For each 
     * variable a weight is generated that varies from [0..1] as the variable 
     * goes from it's valid_min to valid_max (linear). The values are summed 
     * up and divided by the number of variables. 
     */
    template<class T>
    class InvErfcWeightFunction : public WeightFunction<T>
    {
    private:

        map <size_t, T> m_min;
        map <size_t, T> m_max;
        MultiArray <T> *m_weight;

    public:

        /** Construct the weight function, using the given values
         * for valid_min/valid_max
         * @param featurespace
         * @param map of lower bounds
         * @param map of upper bounds
         */
        InvErfcWeightFunction(const detection_params_t<T> &params,
                                                const detection_context_t<T> &ctx)
            : m_weight(new MultiArrayBlitz<T>(ctx.coord_system->get_dimension_sizes(), 0.0))
        {
            // If scale-space filter is present, use the filtered
            // limits. If not, use the original limits
            if (ctx.sf == NULL) {
                for (size_t index = 0; index < ctx.data_store->rank(); index++) {
                    m_min[index] = ctx.data_store->min(index);
                    m_max[index] = ctx.data_store->max(index);
                }
            } else {
                m_min = ctx.sf->get_filtered_min();
                m_max = ctx.sf->get_filtered_max();
            }

            calculate_weight_function(ctx.fs);
        }

        ~InvErfcWeightFunction()
        {
            if (this->m_weight != NULL) {
                delete m_weight;
                m_weight = NULL;
            }
        }

    private:

        void
        calculate_weight_function(const FeatureSpace <T> *fs) {
#if WITH_OPENMP
#pragma omp parallel for 
#endif
            for (size_t i = 0; i < fs->points.size(); i++) {
                Point<T> *p = fs->points[i];
                T saliency = (fs->off_limits()->get(p->gridpoint)) ? 0.0 : compute_weight(fs, p->values);
                m_weight->set(p->gridpoint, saliency);
            }
        };

        /* Actual weight computation happens here */

        T compute_weight(const FeatureSpace <T> *fs, const vector <T> &values) const 
        {
            T sum = 0.0;
            for (size_t var_index = 0; var_index < fs->value_rank(); var_index++) {
                T value = values.at(fs->spatial_rank() + var_index);

                // value scaled to [0..1]
                T max = m_max.find(var_index)->second;
                T min = m_min.find(var_index)->second;
                T var_weight = (value - min) / (max - min);
                sum += var_weight;
            }
            T x = sum / ((T)fs->value_rank());
            return 1 - erfc(2*x);
        }

    public:

        /** @return pre-calculated weight
         */
        T operator()(const typename Point<T>::ptr p) const {
            return m_weight->get(p->gridpoint);
        }
    };
}

#endif
