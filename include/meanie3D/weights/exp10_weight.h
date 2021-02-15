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

#ifndef M3D_EXP10WEIGHTFUNCTION_H
#define M3D_EXP10WEIGHTFUNCTION_H

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

    template<class T>
    class EXP10WeightFunction : public WeightFunction<T>
    {
    private:

        vector<string> m_vars; // variables for weighting
        MultiArray <T> *m_weight;
        const CoordinateSystem <T> *m_coordinate_system;

        void
        calculate_weight_function(FeatureSpace <T> *fs) {
            for (size_t i = 0; i < fs->points.size(); i++) {
                Point<T> *p = fs->points[i];
                T saliency = this->compute_weight(p);
                m_weight->set(p->gridpoint, saliency);
            }
        };

    public:

        /** Construct the weight function, using the default values
         * for valid_min/valid_max
         * @param featurespace
         */
        EXP10WeightFunction(const detection_params_t <T> &params,
                            const detection_context_t <T> &ctx)
                : m_vars(params.variables),
                  m_weight(new MultiArrayBlitz<T>(ctx.coord_system->get_dimension_sizes(), 0.0)),
                  m_coordinate_system(ctx.coord_system) {
            calculate_weight_function(ctx.fs);
        }

        ~EXP10WeightFunction() {
            if (m_weight != NULL) {
                delete m_weight;
                m_weight = NULL;
            }
        }

        /** Actual weight computation happens here
         */
        T compute_weight(Point <T> *p) {
            T sum = 0.0;
            size_t num_vars = p->values.size() - p->coordinate.size();
            for (size_t var_index = 0; var_index < num_vars; var_index++) {
                T value = p->values[p->coordinate.size() + var_index];
                sum += pow(boost::numeric_cast<double>(value), 10.0);
            }
            return sum;
        }

        T operator()(const typename Point<T>::ptr p) const {
            return m_weight->get(p->gridpoint);
        }
    };
}

#endif
