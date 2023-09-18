/**
 * Copyright (c) 2022-2023 Atsushi Tatsuma
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RUMALE_TREE_EXT_HPP
#define RUMALE_TREE_EXT_HPP 1

#include <cmath>
#include <string>
#include <vector>

#include <ruby.h>

#include <numo/narray.h>
#include <numo/template.h>

/**
 * @!visibility private
 * Document-module: Rumale::Tree::ExtDecisionTreeClassifier
 * The mixin module consisting of extension method for DecisionTreeClassifier class.
 * This module is used internally.
 */
class ExtDecisionTreeClassifier {
public:
  static void define_module(VALUE& outer) {
    VALUE rb_mExtDTreeCls = rb_define_module_under(outer, "ExtDecisionTreeClassifier");
    rb_define_private_method(rb_mExtDTreeCls, "find_split_params", find_split_params_, 6);
    rb_define_private_method(rb_mExtDTreeCls, "node_impurity", node_impurity_, 3);
    rb_define_private_method(rb_mExtDTreeCls, "stop_growing?", check_same_label_, 1);
  }

private:
  static double calc_impurity_(const std::string& criterion, const std::vector<size_t>& histogram, const size_t& n_elements, const size_t& n_classes) {
    double impurity = 0.0;
    if (criterion == "entropy") {
      double entropy = 0.0;
      for (size_t i = 0; i < n_classes; i++) {
        const double el = static_cast<double>(histogram[i]) / static_cast<double>(n_elements);
        entropy += el * std::log(el + 1.0);
      }
      impurity = -entropy;
    } else {
      double gini = 0.0;
      for (size_t i = 0; i < n_classes; i++) {
        const double el = static_cast<double>(histogram[i]) / static_cast<double>(n_elements);
        gini += el * el;
      }
      impurity = 1.0 - gini;
    }
    return impurity;
  }

  /**
   * @!visibility private
   * Find for split point with maximum information gain.
   *
   * @overload find_split_params(criterion, impurity, order, features, labels, n_classes) -> Array<Float>
   *
   * @param criterion [String] The function to evaluate spliting point. Supported criteria are 'gini' and 'entropy'.
   * @param impurity [Float] The impurity of whole dataset.
   * @param order [Numo::Int32] (shape: [n_elements]) The element indices sorted according to feature values.
   * @param features [Numo::DFloat] (shape: [n_elements]) The feature values.
   * @param labels [Numo::Int32] (shape: [n_elements]) The labels.
   * @param n_classes [Integer] The number of classes.
   * @return [Array<Float>] The array consists of optimal parameters including impurities of child nodes, threshold, and gain.
   */

  struct FindSplitParamsOpts_ {
    std::string criterion;
    size_t n_classes;
    double impurity;
  };

  static void iter_find_split_params_(na_loop_t const* lp) {
    // Obtain iteration variables.
    const int32_t* o = (int32_t*)NDL_PTR(lp, 0);
    const size_t n_elements = NDL_SHAPE(lp, 0)[0];
    const double* f = (double*)NDL_PTR(lp, 1);
    const int32_t* y = (int32_t*)NDL_PTR(lp, 2);
    const std::string criterion = ((FindSplitParamsOpts_*)lp->opt_ptr)->criterion;
    const size_t n_classes = ((FindSplitParamsOpts_*)lp->opt_ptr)->n_classes;
    const double w_impurity = ((FindSplitParamsOpts_*)lp->opt_ptr)->impurity;

    // Initialize output optimal parameters.
    double* params = (double*)NDL_PTR(lp, 3);
    params[0] = 0.0;        // left impurity
    params[1] = w_impurity; // right impurity
    params[2] = f[o[0]];    // threshold
    params[3] = 0.0;        // gain

    // Initialize child node variables.
    std::vector<size_t> r_histogram(n_classes, 0);
    for (size_t i = 0; i < n_elements; i++) r_histogram[y[o[i]]] += 1;

    // Find optimal parameters.
    size_t curr_pos = 0;
    size_t next_pos = 0;
    size_t n_l_elements = 0;
    size_t n_r_elements = n_elements;
    double curr_el = f[o[0]];
    const double last_el = f[o[n_elements - 1]];
    std::vector<size_t> l_histogram(n_classes, 0);
    while (curr_pos < n_elements && curr_el != last_el) {
      double next_el = f[o[next_pos]];
      while (next_pos < n_elements && next_el == curr_el) {
        l_histogram[y[o[next_pos]]] += 1;
        n_l_elements++;
        r_histogram[y[o[next_pos]]] -= 1;
        n_r_elements--;
        next_pos++;
        next_el = f[o[next_pos]];
      }
      // Calculate gain of new split.
      const double l_impurity = calc_impurity_(criterion, l_histogram, n_l_elements, n_classes);
      const double r_impurity = calc_impurity_(criterion, r_histogram, n_r_elements, n_classes);
      const double gain = w_impurity - (n_l_elements * l_impurity + n_r_elements * r_impurity) / static_cast<double>(n_elements);
      // Update optimal parameters.
      if (gain > params[3]) {
        params[0] = l_impurity;
        params[1] = r_impurity;
        params[2] = 0.5 * (curr_el + next_el);
        params[3] = gain;
      }
      if (next_pos == n_elements) break;
      curr_pos = next_pos;
      curr_el = f[o[curr_pos]];
    }
  }

  static VALUE find_split_params_(VALUE self, VALUE criterion, VALUE impurity, VALUE order, VALUE features, VALUE labels, VALUE n_classes) {
    ndfunc_arg_in_t ain[3] = { { numo_cInt32, 1 }, { numo_cDFloat, 1 }, { numo_cInt32, 1 } };
    size_t out_shape[1] = { 4 };
    ndfunc_arg_out_t aout[1] = { { numo_cDFloat, 1, out_shape } };
    ndfunc_t ndf = { (na_iter_func_t)iter_find_split_params_, NO_LOOP, 3, 1, ain, aout };
    FindSplitParamsOpts_ opts = { std::string(StringValueCStr(criterion)), NUM2SIZET(n_classes), NUM2DBL(impurity) };
    VALUE params = na_ndloop3(&ndf, &opts, 3, order, features, labels);
    RB_GC_GUARD(criterion);
    return params;
  }

  /**
   * @!visibility private
   * Calculate impurity based on criterion.
   *
   * @overload node_impurity(criterion, y, n_classes) -> Float
   *
   * @param criterion [String] The function to calculate impurity. Supported criteria are 'gini' and 'entropy'.
   * @param y [Numo::Int32] (shape: [n_samples]) The labels.
   * @param n_classes [Integer] The number of classes.
   * @return [Float] impurity
   */

  struct NodeImpurityOpts_ {
    std::string criterion;
    size_t n_classes;
  };

  static void iter_node_impurity_(na_loop_t const* lp) {
    const int32_t* y = (int32_t*)NDL_PTR(lp, 0);
    const size_t n_elements = NDL_SHAPE(lp, 0)[0];
    const std::string criterion = ((NodeImpurityOpts_*)lp->opt_ptr)->criterion;
    const size_t n_classes = ((NodeImpurityOpts_*)lp->opt_ptr)->n_classes;
    double* ret = (double*)NDL_PTR(lp, 1);
    std::vector<size_t> histogram(n_classes, 0);
    for (size_t i = 0; i < n_elements; i++) histogram[y[i]] += 1;
    *ret = calc_impurity_(criterion, histogram, n_elements, n_classes);
  }

  static VALUE node_impurity_(VALUE self, VALUE criterion, VALUE y, VALUE n_classes) {
    ndfunc_arg_in_t ain[1] = { { numo_cInt32, 1 } };
    ndfunc_arg_out_t aout[1] = { { numo_cDFloat, 0 } };
    ndfunc_t ndf = { (na_iter_func_t)iter_node_impurity_, NO_LOOP | NDF_EXTRACT, 1, 1, ain, aout };
    NodeImpurityOpts_ opts = { std::string(StringValueCStr(criterion)), NUM2SIZET(n_classes) };
    VALUE ret = na_ndloop3(&ndf, &opts, 1, y);
    RB_GC_GUARD(criterion);
    return ret;
  }

  /**
   * @!visibility private
   * Check all elements have the save value.
   *
   * @overload check_same_label(y) -> Boolean
   *
   * @param y [Numo::Int32] (shape: [n_samples]) The labels.
   * @return [Boolean]
   */

  static void iter_check_same_label_(na_loop_t const* lp) {
    const int32_t* y = (int32_t*)NDL_PTR(lp, 0);
    const size_t n_elements = NDL_SHAPE(lp, 0)[0];
    VALUE* ret = (VALUE*)NDL_PTR(lp, 1);
    *ret = Qtrue;
    if (n_elements > 0) {
      int32_t label = y[0];
      for (size_t i = 0; i < n_elements; i++) {
        if (y[i] != label) {
          *ret = Qfalse;
          break;
        }
      }
    }
  }

  static VALUE check_same_label_(VALUE self, VALUE y) {
    ndfunc_arg_in_t ain[1] = { { numo_cInt32, 1 } };
    ndfunc_arg_out_t aout[1] = { { numo_cRObject, 0 } };
    ndfunc_t ndf = { (na_iter_func_t)iter_check_same_label_, NO_LOOP | NDF_EXTRACT, 1, 1, ain, aout };
    return na_ndloop(&ndf, 1, y);
  }
};

/**
 * @!visibility private
 * Document-module: Rumale::Tree::ExtDecisionTreeRegressor
 * The mixin module consisting of extension method for DecisionTreeRegressor class.
 * This module is used internally.
 */
class ExtDecisionTreeRegressor {
public:
  static void define_module(VALUE& outer) {
    VALUE rb_mExtDTreeReg = rb_define_module_under(outer, "ExtDecisionTreeRegressor");
    rb_define_private_method(rb_mExtDTreeReg, "find_split_params", find_split_params_, 5);
    rb_define_private_method(rb_mExtDTreeReg, "node_impurity", node_impurity_, 2);
  }

private:
  static double calc_impurity_(const std::string& criterion, const std::vector<std::vector<double> >& target_vecs, const std::vector<double>& sum_vec) {
    const size_t n_elements = target_vecs.size();
    const size_t n_outputs = target_vecs[0].size();
    std::vector<double> mean_vec(n_outputs, 0);
    for (size_t j = 0; j < n_outputs; j++) mean_vec[j] = sum_vec[j] / static_cast<double>(n_elements);

    double impurity = 0.0;
    if (criterion == "mae") {
      double sum_mae = 0.0;
      for (size_t i = 0; i < n_elements; i++) {
        double mae = 0.0;
        for (size_t j = 0; j < n_outputs; j++) {
          const double el = target_vecs[i][j] - mean_vec[j];
          mae += std::fabs(el);
        }
        mae /= static_cast<double>(n_outputs);
        sum_mae += mae;
      }
      impurity = sum_mae / static_cast<double>(n_elements);
    } else {
      double sum_mse = 0.0;
      for (size_t i = 0; i < n_elements; i++) {
        double mse = 0.0;
        for (size_t j = 0; j < n_outputs; j++) {
          const double el = target_vecs[i][j] - mean_vec[j];
          mse += el * el;
        }
        mse /= static_cast<double>(n_outputs);
        sum_mse += mse;
      }
      impurity = sum_mse / static_cast<double>(n_elements);
    }
    return impurity;
  }

  /**
   * @!visibility private
   * Find for split point with maximum information gain.
   *
   * @overload find_split_params(criterion, impurity, order, features, targets) -> Array<Float>
   *
   * @param criterion [String] The function to evaluate spliting point. Supported criteria are 'mae' and 'mse'.
   * @param impurity [Float] The impurity of whole dataset.
   * @param order [Numo::Int32] (shape: [n_samples]) The element indices sorted according to feature values in ascending order.
   * @param features [Numo::DFloat] (shape: [n_samples]) The feature values.
   * @param targets [Numo::DFloat] (shape: [n_samples, n_outputs]) The target values.
   * @return [Array<Float>] The array consists of optimal parameters including impurities of child nodes, threshold, and gain.
   */

  struct FindSplitParamsOpts_ {
    std::string criterion;
    double impurity;
  };

  static void iter_find_split_params_(na_loop_t const* lp) {
    // Obtain iteration variables.
    const int32_t* o = (int32_t*)NDL_PTR(lp, 0);
    const size_t n_elements = NDL_SHAPE(lp, 0)[0];
    const double* f = (double*)NDL_PTR(lp, 1);
    const double* y = (double*)NDL_PTR(lp, 2);
    const size_t n_outputs = NDL_SHAPE(lp, 2)[1];
    const std::string criterion = ((FindSplitParamsOpts_*)lp->opt_ptr)->criterion;
    const double w_impurity = ((FindSplitParamsOpts_*)lp->opt_ptr)->impurity;

    // Initialize optimal parameters.
    double* params = (double*)NDL_PTR(lp, 3);
    params[0] = 0.0;        // left impurity
    params[1] = w_impurity; // right impurity
    params[2] = f[o[0]];    // threshold
    params[3] = 0.0;        // gain

    // Initialize child node variables.
    std::vector<double> r_sum_vec(n_outputs, 0);
    std::vector<std::vector<double> > r_target_vecs;
    r_target_vecs.reserve(n_elements);
    for (size_t i = 0; i < n_elements; i++) {
      std::vector<double> target(n_outputs, 0);
      for (size_t j = 0; j < n_outputs; j++) {
        const double el = y[o[i] * n_outputs + j];
        r_sum_vec[j] += el;
        target[j] = el;
      }
      r_target_vecs.push_back(target);
    }

    // Find optimal parameters.
    size_t curr_pos = 0;
    size_t next_pos = 0;
    size_t n_l_elements = 0;
    size_t n_r_elements = n_elements;
    double curr_el = f[o[0]];
    const double last_el = f[o[n_elements - 1]];
    std::vector<double> l_sum_vec(n_outputs, 0);
    std::vector<std::vector<double> > l_target_vecs;
    while (curr_pos < n_elements && curr_el != last_el) {
      double next_el = f[o[next_pos]];
      while (next_pos < n_elements && next_el == curr_el) {
        std::vector<double> target = r_target_vecs[0];
        r_target_vecs.erase(r_target_vecs.begin());
        for (size_t j = 0; j < n_outputs; j++) r_sum_vec[j] -= target[j];
        n_r_elements--;
        l_target_vecs.push_back(target);
        for (size_t j = 0; j < n_outputs; j++) l_sum_vec[j] += target[j];
        n_l_elements++;
        next_pos++;
        next_el = f[o[next_pos]];
      }
      // Calculate gain of new split.
      const double l_impurity = calc_impurity_(criterion, l_target_vecs, l_sum_vec);
      const double r_impurity = calc_impurity_(criterion, r_target_vecs, r_sum_vec);
      const double gain = w_impurity - (n_l_elements * l_impurity + n_r_elements * r_impurity) / static_cast<double>(n_elements);
      // Update optimal parameters.
      if (gain > params[3]) {
        params[0] = l_impurity;
        params[1] = r_impurity;
        params[2] = 0.5 * (curr_el + next_el);
        params[3] = gain;
      }
      if (next_pos == n_elements) break;
      curr_pos = next_pos;
      curr_el = f[o[curr_pos]];
    }
  }

  static VALUE find_split_params_(VALUE self, VALUE criterion, VALUE impurity, VALUE order, VALUE features, VALUE targets) {
    ndfunc_arg_in_t ain[3] = { { numo_cInt32, 1 }, { numo_cDFloat, 1 }, { numo_cDFloat, 2 } };
    size_t out_shape[1] = { 4 };
    ndfunc_arg_out_t aout[1] = { { numo_cDFloat, 1, out_shape } };
    ndfunc_t ndf = { (na_iter_func_t)iter_find_split_params_, NO_LOOP, 3, 1, ain, aout };
    FindSplitParamsOpts_ opts = { std::string(StringValueCStr(criterion)), NUM2DBL(impurity) };
    VALUE params = na_ndloop3(&ndf, &opts, 3, order, features, targets);
    RB_GC_GUARD(criterion);
    return params;
  }

  /**
   * @!visibility private
   * Calculate impurity based on criterion.
   *
   * @overload node_impurity(criterion, y) -> Float
   *
   * @param criterion [String] The function to calculate impurity. Supported criteria are 'mae' and 'mse'.
   * @param y [Array<Float>] (shape: [n_samples, n_outputs]) The taget values.
   * @return [Float] impurity
   */

  struct NodeImpurityOpts_ {
    std::string criterion;
  };

  static void iter_node_impurity_(na_loop_t const* lp) {
    const double* y = (double*)NDL_PTR(lp, 0);
    const size_t n_elements = NDL_SHAPE(lp, 0)[0];
    const size_t n_outputs = NDL_SHAPE(lp, 0)[1];
    const std::string criterion = ((NodeImpurityOpts_*)lp->opt_ptr)->criterion;
    double* ret = (double*)NDL_PTR(lp, 1);

    std::vector<double> sum_vec(n_outputs, 0);
    std::vector<std::vector<double> > target_vecs;
    target_vecs.reserve(n_elements);
    for (size_t r = 0; r < n_elements; r++) {
      std::vector<double> target(n_outputs, 0);
      for (size_t c = 0; c < n_outputs; c++) {
        const double el = y[r * n_outputs + c];
        sum_vec[c] += el;
        target[c] = el;
      }
      target_vecs.push_back(target);
    }

    *ret = calc_impurity_(criterion, target_vecs, sum_vec);
  }

  static VALUE node_impurity_(VALUE self, VALUE criterion, VALUE y) {
    ndfunc_arg_in_t ain[1] = { { numo_cDFloat, 2 } };
    ndfunc_arg_out_t aout[1] = { { numo_cDFloat, 0 } };
    ndfunc_t ndf = { (na_iter_func_t)iter_node_impurity_, NO_LOOP | NDF_EXTRACT, 1, 1, ain, aout };
    NodeImpurityOpts_ opts = { std::string(StringValueCStr(criterion)) };
    VALUE ret = na_ndloop3(&ndf, &opts, 1, y);
    RB_GC_GUARD(criterion);
    return ret;
  }
};

/**
 * @!visibility private
 * Document-module: Rumale::Tree::ExtGradientTreeRegressor
 * The mixin module consisting of extension method for GradientTreeRegressor class.
 * This module is used internally.
 */
class ExtGradientTreeRegressor {
public:
  static void define_module(VALUE& outer) {
    VALUE rb_mExtGTreeReg = rb_define_module_under(outer, "ExtGradientTreeRegressor");
    rb_define_private_method(rb_mExtGTreeReg, "find_split_params", find_split_params_, 7);
  }

private:
  /**
   * @!visibility private
   * Find for split point with maximum information gain.
   *
   * @overload find_split_params(order, features, gradients, hessians, sum_gradient, sum_hessian, reg_lambda) -> Array<Float>
   *   @param order [Numo::Int32] (shape: [n_elements]) The element indices sorted according to feature values.
   *   @param features [Numo::DFloat] (shape: [n_elements]) The feature values.
   *   @param gradients [Numo::DFloat] (shape: [n_elements]) The gradient values.
   *   @param hessians [Numo::DFloat] (shape: [n_elements]) The hessian values.
   *   @param sum_gradient [Float] The sum of gradient values.
   *   @param sum_hessian [Float] The sum of hessian values.
   *   @param reg_lambda [Float] The L2 regularization term on weight.
   * @return [Array<Float>] The array consists of optimal parameters including threshold and gain.
   */

  struct FindSplitParamsOpts_ {
    double sum_gradient;
    double sum_hessian;
    double reg_lambda;
  };

  static void iter_find_split_params_(na_loop_t const* lp) {
    // Obtain iteration variables.
    const int32_t* o = (int32_t*)NDL_PTR(lp, 0);
    const size_t n_elements = NDL_SHAPE(lp, 0)[0];
    const double* f = (double*)NDL_PTR(lp, 1);
    const double* g = (double*)NDL_PTR(lp, 2);
    const double* h = (double*)NDL_PTR(lp, 3);
    const double s_grad = ((FindSplitParamsOpts_*)lp->opt_ptr)->sum_gradient;
    const double s_hess = ((FindSplitParamsOpts_*)lp->opt_ptr)->sum_hessian;
    const double reg_lambda = ((FindSplitParamsOpts_*)lp->opt_ptr)->reg_lambda;

    // Find optimal parameters.
    size_t curr_pos = 0;
    size_t next_pos = 0;
    double curr_el = f[o[0]];
    const double last_el = f[o[n_elements - 1]];
    double l_grad = 0.0;
    double l_hess = 0.0;
    double threshold = curr_el;
    double gain_max = 0.0;
    while (curr_pos < n_elements && curr_el != last_el) {
      double next_el = f[o[next_pos]];
      while (next_pos < n_elements && next_el == curr_el) {
        l_grad += g[o[next_pos]];
        l_hess += h[o[next_pos]];
        next_pos++;
        next_el = f[o[next_pos]];
      }
      // Calculate gain of new split.
      const double r_grad = s_grad - l_grad;
      const double r_hess = s_hess - l_hess;
      const double gain = (l_grad * l_grad) / (l_hess + reg_lambda) + (r_grad * r_grad) / (r_hess + reg_lambda) - (s_grad * s_grad) / (s_hess + reg_lambda);
      // Update optimal parameters.
      if (gain > gain_max) {
        threshold = 0.5 * (curr_el + next_el);
        gain_max = gain;
      }
      if (next_pos == n_elements) break;
      curr_pos = next_pos;
      curr_el = f[o[curr_pos]];
    }

    double* params = (double*)NDL_PTR(lp, 4);
    params[0] = threshold;
    params[1] = gain_max;
  }

  static VALUE find_split_params_(VALUE self, VALUE order, VALUE features, VALUE gradients, VALUE hessians,
                                  VALUE sum_gradient, VALUE sum_hessian, VALUE reg_lambda) {
    ndfunc_arg_in_t ain[4] = { { numo_cInt32, 1 }, { numo_cDFloat, 1 }, { numo_cDFloat, 1 }, { numo_cDFloat, 1 } };
    size_t out_shape[1] = { 2 };
    ndfunc_arg_out_t aout[1] = { { numo_cDFloat, 1, out_shape } };
    ndfunc_t ndf = { (na_iter_func_t)iter_find_split_params_, NO_LOOP, 4, 1, ain, aout };
    FindSplitParamsOpts_ opts = { NUM2DBL(sum_gradient), NUM2DBL(sum_hessian), NUM2DBL(reg_lambda) };
    VALUE params = na_ndloop3(&ndf, &opts, 4, order, features, gradients, hessians);
    return params;
  }
};

#endif /* RUMALE_TREE_EXT_HPP */
