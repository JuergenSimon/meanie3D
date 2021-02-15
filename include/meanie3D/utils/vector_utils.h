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

#ifndef M3D_VECTOR_UTILS_H
#define M3D_VECTOR_UTILS_H

#include <meanie3D/defines.h>
#include <meanie3D/namespaces.h>

#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <iostream>
#include <math.h>
#include <cmath>
#include <assert.h>
#include <vector>
#include <sstream>

namespace std {

    // Convenience operator << for printing vectors out to streams

    template<class T>
    std::ostream &operator<<(std::ostream &os, const vector<T> &v) {
        size_t count = 0;
        os << "(";
        for (typename vector<T>::const_iterator ii = v.begin(); ii != v.end(); ++ii) {
            os << *ii;

            if (count < (v.size() - 1)) {
                os << ",";
            }
            count++;
        }
        os << ")";
        return os;
    }
}

namespace m3D {
    namespace utils {
        namespace vectors {

            using std::vector;

            // - operator

            template<class T>
            inline
            const vector<T>
            operator-(const vector<T> &v1, const vector<T> &v2) {
#if ASSERT_VECTOR_SIZES
                assert(v1.size() == v2.size());
#endif
                vector<T> result(v1.size());
                for (size_t i = 0; i < v1.size(); i++) {
                    result[i] = v1[i] - v2[i];
                }
                return result;
            }

            // -= operator

            template<class T>
            inline
            void
            operator-=(vector<T> &v1, const vector<T> &v2) {
#if ASSERT_VECTOR_SIZES
                assert(v1.size() == v2.size());
#endif
                for (size_t i = 0; i < v1.size(); i++) {
                    v1[i] -= v2[i];
                }
            }

            // + operator

            template<class T>
            inline
            const vector<T>
            operator+(const vector<T> &v1, const vector<T> &v2) {
#if ASSERT_VECTOR_SIZES
                assert(v1.size() == v2.size());
#endif
                vector<T> result(v1.size());
                for (size_t i = 0; i < v1.size(); i++) {
                    result[i] = v1[i] + v2[i];
                }
                return result;
            }

            // += operator

            template<class T>
            inline
            void
            operator+=(vector<T> &v1, const vector<T> &v2) {
#if ASSERT_VECTOR_SIZES
                assert(v1.size() == v2.size());
#endif
                for (size_t i = 0; i < v1.size(); i++) {
                    v1[i] += v2[i];
                }
            }

            // * operator (scalar multiplication)

            template<class T>
            inline
            vector<T>
            operator*(const vector<T> &v1, const T s) {
                vector<T> result(v1.size());
                for (size_t i = 0; i < v1.size(); i++) {
                    result[i] = s * v1[i];
                }
                return result;
            }

            template<class T>
            inline
            vector<T>
            operator*(const T s, const vector<T> &v1) {
                vector<T> result(v1.size());
                for (size_t i = 0; i < v1.size(); i++) {
                    result[i] = s * v1[i];
                }
                return result;
            }


            // *= operator (scalar multiplication)

            template<class T>
            inline
            void
            operator*=(vector<T> &v1, const T s) {
                for (size_t i = 0; i < v1.size(); i++) {
                    v1[i] *= s;
                }
            }

            // /= operator (multiplication with inverse of scalar)

            template<class T>
            inline
            void
            operator/=(vector<T> &v1, const T s) {
                for (size_t i = 0; i < v1.size(); i++) {
                    v1[i] /= s;
                }
            }

            // / operator (multiplication with inverse of scalar)

            template<class T>
            inline
            vector<T>
            operator/(const vector<T> &v1, const T s) {
                vector<T> result(v1.size());
                for (size_t i = 0; i < v1.size(); i++) {
                    result[i] = v1[i] / s;
                }
                return result;
            }

            // * operator (scalar product of two vectors)

            template<typename T>
            inline
            T
            operator*(const vector<T> v1, const vector<T> v2) {
#if ASSERT_VECTOR_SIZES
                assert(v1.size() == v2.size());
#endif
                T result = 0.0;
                for (size_t k = 0; k < v1.size(); k++) {
                    result += v1[k] * v2[k];
                }
                return result;
            }

            // norm

            template<typename T>
            inline
            T
            vector_norm(const vector<T> *v) {
                double sum = 0;
                typename vector<T>::const_iterator i;
                for (i = v->begin(); i != v->end(); i++) {
                    sum += (*i) * (*i);
                }
                T result = (T) sqrt(sum);
                return result;
            }

            template<typename T>
            inline
            T
            vector_norm(const vector<T> &v) {
                double sum = 0;
                typename vector<T>::const_iterator i;
                for (i = v.begin(); i != v.end(); i++) {
                    sum += (*i) * (*i);
                }
                T result = (T) sqrt(sum);
                return result;
            }

            template<typename T>
            inline
            bool
            vector_is_null(vector<T> *v) {
                typename vector<T>::iterator it;
                for (it = v->begin(); it != v->end(); it++) {
                    if (*it != 0) return false;
                }
                return true;
            }

            template<typename T>
            inline
            bool
            vector_is_null(vector<T> &v) {
                typename vector<T>::iterator it;
                for (it = v.begin(); it != v.end(); it++) {
                    if (*it != 0) return false;
                }
                return true;
            }

            template<typename T>
            inline
            bool
            within_range(const vector<T> &x, const vector<T> &y, const vector<T> &h) {
#if ASSERT_VECTOR_SIZES
                assert(x.size() == y.size());
                assert(x.size() == h.size());
#endif
                bool within_range = true;
                for (size_t i = 0; i < x.size() && within_range; i++) {
                    within_range = (fabs(x[i] - y[i]) <= h[i]);
                }
                return within_range;
            }

            template<typename T>
            inline
            bool
            closer_than(const vector<T> &x, const vector<T> &y, const vector<T> &h) {
#if ASSERT_VECTOR_SIZES
                assert(x.size() == y.size());
                assert(x.size() == h.size());
#endif
                bool closer = true;
                for (size_t i = 0; i < x.size() && closer; i++) {
                    closer = (fabs(x[i] - y[i]) < h[i]);
                }
                return closer;
            }

            template<typename T>
            inline
            T
            vector_angle(vector<T> &v1, vector<T> &v2) {
#if ASSERT_VECTOR_SIZES
                assert(v1.size() == v2.size());
#endif
                return acos((v1 * v2) / (vector_norm(v1) * vector_norm(v2)));
            }

            template<typename T>
            inline
            T
            mahalabonis_distance_sqr(const vector<T> &x, const vector<T> &y, const vector<T> &H) {
#if ASSERT_VECTOR_SIZES
                assert(x.size() == y.size());
                assert(x.size() == H.size());
#endif
                T result = 0.0;
                for (size_t k = 0; k < x.size(); k++) {
                    if (H[k] > 0) {
                        result += (x[k] - y[k]) * (x[k] - y[k]) / (H[k] * H[k]);
                    }
                }

                return result;
            }

            template<typename T>
            inline
            /* @param x
             * @param y
             * @return (x1 * y1, x2 * y2, ... , xn * yn ) ( x * diag(y) )
             */
            vector<T> vector_diagonal_product(vector<T> &x, vector<T> &y) {
#if ASSERT_VECTOR_SIZES
                assert(x.size() == y.size());
#endif
                vector<T> r(x.size());
                for (size_t k = 0; k < x.size(); k++) {
                    r[k] = x[k] * y[k];
                }
                return r;
            }

            /** Returns the index of the first element that is equal to the given element. 
             * Courtesy of http://www.dreamincode.net/forums/topic/36630-find-index-of-vector-element/
             * @param vector
             * @param element
             * @return index of the first element matching the given one or -1 if no matching element was found.
             */
            template<typename T>
            inline
            const int
            index_of_first(const vector<T> &myvec, const T &elem) {
                int pos = -1;

                for (int i = 0; i < myvec.size(); i++) {
                    if (myvec[i] == elem) {
                        pos = i;
                        break;
                    }
                }

                return pos;
            }

            /** Converts the given vector to a string of the form
             * (v1,v2,...,vn)
             * 
             * @param v
             * @return 
             */
            template<typename T>
            std::string
            to_string(const std::vector<T> &v) {
                std::stringstream str(std::stringstream::in | std::stringstream::out);
                str << "(";
                for (size_t i = 0; i < v.size(); i++) {
                    str << v[i];
                    if (i < (v.size() - 1)) {
                        str << ",";
                    }
                };
                str << ")";
                return str.str();
            }

            /** Converts the given string in the form 
             * (v1,v2,...,vn) into a vector. 
             * 
             * @param const_str
             * @return 
             */
            template<typename T>
            std::vector<T>
            from_string(const std::string &const_str) {
                vector<T> result;
                std::string str = const_str;

                // Decode mode
                typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
                boost::char_separator<char> sep(",");
                char chars[] = "()";
                for (unsigned int i = 0; i < strlen(chars); ++i) {
                    // you need include <algorithm> to use general algorithms like std::remove()
                    str.erase(std::remove(str.begin(), str.end(), chars[i]), str.end());
                }

                tokenizer tokens(str, sep);
                vector<T> mode;
                for (tokenizer::iterator tok_iter = tokens.begin(); tok_iter != tokens.end(); ++tok_iter) {
                    std::string token = *tok_iter;
                    T value;
                    try {
                        value = boost::lexical_cast<T>(token);
                        result.push_back(value);
                    }
                    catch (std::exception &e)
                    {
                        std::cerr << "Could not convert token " << token << std::endl;
                        // NOTE: this is a workaround for the problem described in
                        // https: //stackoverflow.com/questions/62553744/why-does-boostlexical-cast-throw-an-exception-even-though-it-converted-the-val
                        result.push_back(value);
                    }
                }

                return result;
            }

            /** JSON encodes the given vector
             * 
             * @param v
             * @return 
             */
            template<class T>
            std::string to_json(const vector<T> &v) {
                std::ostringstream out;
                out << "[";
                size_t count = 0;
                for (typename vector<T>::const_iterator ii = v.begin(); ii != v.end(); ++ii) {
                    out << *ii;
                    if (count < (v.size() - 1)) out << ",";
                    count++;
                }
                out << "]";
                return out.str();
            }

            /** JSON encodes the given vector of strings
             * 
             * @param v
             * @return 
             */
            std::string to_json(const std::vector<std::string> &v) {
                std::ostringstream out;

                out << "[";

                size_t count = 0;

                std::vector<std::string>::const_iterator ii;

                for (ii = v.begin(); ii != v.end(); ++ii) {
                    out << "\"" << *ii << "\"";

                    if (count < (v.size() - 1)) out << ",";

                    count++;
                }

                out << "]";

                return out.str();
            }

        }
    }
}

#endif
