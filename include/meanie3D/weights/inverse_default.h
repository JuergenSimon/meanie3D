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

#ifndef M3D_INVERSEDEFAULTWEIGHTFUNCTION_H
#define M3D_INVERSEDEFAULTWEIGHTFUNCTION_H

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>
#include <meanie3D/utils.h>
#include <meanie3D/weights/weight_function.h>

#include <netcdf>
#include <vector>
#include <map>

namespace m3D {

    using namespace netCDF;
    using namespace ::m3D;
    using std::vector;
    using std::map;

    /**
     * This weight function is the inverse function to the default weight
     * function:
     * 
     * f(x) = 1 - x
     * 
     * It varies from 1 to 0 as each variable in the value range goes from 
     * it's minimum to it's maximum. It can be useful if the goal is to track 
     * 'holes' rather than 'mountains'. The overall weight is again the arithmetic
     * mean of all weights in the value range.
     * 
     */
    template<class T>
    class InverseDefaultWeightFunction : public WeightFunction<T>
    {
    protected:

        vector<string> m_vars; // variables for weighting
        map<size_t, T> m_min; // [index,min]
        map<size_t, T> m_max; // [index,max]
        MultiArray <T> *m_weight;
        const CoordinateSystem <T> *m_coordinate_system;

        void
        calculate_weight_function(FeatureSpace <T> *fs) {
            for (size_t i = 0; i < fs->points.size(); i++) {
                Point<T> *p = fs->points[i];
                T saliency = compute_weight(p);
                m_weight->set(p->gridpoint, saliency);
            }
        };

    public:

        /** Construct the weight function, using the default values
         * for valid_min/valid_max
         * @param featurespace
         */
        InverseDefaultWeightFunction(const detection_params_t <T> &params,
                                     const detection_context_t <T> &ctx)
                : m_vars(params.variables),
                  m_weight(new MultiArrayBlitz<T>(ctx.coord_system->get_dimension_sizes(), 0.0)),
                  m_coordinate_system(ctx.coord_system) {
            // If scale-space filter is present, use the filtered
            // limits. If not, use the original limits
            if (ctx.sf == NULL) {
                for (size_t index = 0; index < m_vars.size(); index++) {
                    m_min[index] = ctx.data_store->min(index);
                    m_max[index] = ctx.data_store->max(index);
                }
            } else {
                m_min = ctx.sf->get_filtered_min();
                m_max = ctx.sf->get_filtered_max();
            }

            calculate_weight_function(ctx.fs);
        }

        ~InverseDefaultWeightFunction() {
            if (this->m_weight != NULL) {
                delete m_weight;
                m_weight = NULL;
            }
        }

        /** Actual weight computation happens here
         */
        virtual T compute_weight(Point <T> *p) {
            T sum = 0.0;
            size_t num_vars = p->values.size() - p->coordinate.size();
            if (p->isOriginalPoint) {
                for (size_t var_index = 0; var_index < num_vars; var_index++) {
                    T value = p->values[p->coordinate.size() + var_index];
                    // value scaled to [0..1]
                    T a = -1.0 / (m_max.at(var_index) - m_min.at(var_index));
                    T b = 0.5 * (1.0 - a * (m_max.at(var_index) - m_min.at(var_index)));
                    T var_weight = a * value + b;
                    sum += var_weight;
                }
            }
            return sum / num_vars;
        }

        T operator()(const typename Point<T>::ptr p) const {
            return m_weight->get(p->gridpoint);
        }
    };
}

#endif
