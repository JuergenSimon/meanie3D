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

#ifndef M3D_WEIGHTTHRESHOLDFILTER_IMPL_H
#define M3D_WEIGHTTHRESHOLDFILTER_IMPL_H

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>
#include <meanie3D/utils.h>

#include "weight_filter.h"

namespace m3D {

    template<typename T>
    WeightThresholdFilter<T>::WeightThresholdFilter(WeightFunction <T> *w,
                                                    T lower,
                                                    T upper,
                                                    bool show_progress)
            : FeatureSpaceFilter<T>(show_progress), m_weight_function(w), m_lower_threshold(lower),
              m_upper_threshold(upper) {
    }

    template<typename T>
    WeightThresholdFilter<T>::~WeightThresholdFilter() {
    }

#pragma mark -
#pragma mark Abstract filter method

    template<typename T>
    void
    WeightThresholdFilter<T>::apply(FeatureSpace <T> *fs) {
        using namespace std;
        boost::progress_display *progress_bar = NULL;
        if (this->show_progress()) {
            cout << endl << "Applying weight function filter ...";
            progress_bar = new boost::progress_display(fs->size());
            start_timer();
        }
        typename Point<T>::list::iterator pit;
        vector<Point<T> *> accepted;
        vector<Point<T> *> erased;

        // // Calculate the max and min values of the weight function response
        // T w_max = -10e10;
        // T w_min = 10e10;
        // for (size_t k = 0; k < fs->points.size(); k++) 
        // {
        //     Point<T> *p = fs->points.at(k);
        //     T w = m_weight_function->operator()(p);
        //     if (w > w_max) {
        //         w_max = w;
        //     }
        //     if (w < w_min)
        //     {
        //         w_min = w;
        //     }
        // }

        // // The filter criteria refer to percentages in the weight function's range
        // T weight_range = w_max - w_min;
        // T lower_bound = w_min + weight_range * m_lower_threshold;
        // T upper_bound = w_min + weight_range * m_upper_threshold;

#if WITH_OPENMP
#pragma omp parallel for schedule(dynamic, 10)
#endif
        for (size_t k = 0; k < fs->points.size(); k++)
        {
            if (this->show_progress()) {
                progress_bar->operator++();
            }
            Point<T> *p = fs->points.at(k);
            T w = m_weight_function->operator()(p);
            if (w < m_lower_threshold || w > m_upper_threshold)
            {
#if WITH_OPENMP
#pragma omp critical
#endif
                erased.push_back(p);
            }
            else
            {
#if WITH_OPENMP
#pragma omp critical
#endif
                accepted.push_back(p);
            }
        }

        fs->points = accepted;

#if WITH_OPENMP
#pragma omp parallel for schedule(dynamic, 10)
#endif
        for (size_t i = 0; i < erased.size(); i++)
        {
            Point<T> *p = erased[i];
            delete p;
        }

        if (this->show_progress()) {
            delete progress_bar;
            cout << "done. (" << stop_timer() << "s)" << std::endl;
        }
    }
}

#endif
