/*
 * @BEGIN LICENSE
 *
 * Forte: an open-source plugin to Psi4 (https://github.com/psi4/psi4)
 * that implements a variety of quantum chemistry methods for strongly
 * correlated electrons.
 *
 * Copyright (c) 2012-2025 by its authors (see COPYING, COPYING.LESSER, AUTHORS).
 *
 * The copyrights for code used from other parties are included in
 * the corresponding files.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 *
 * @END LICENSE
 */

#pragma once

#include <vector>
#include <unordered_map>
#include <helpers/math_structures.h>

#include "sparse_ci/sparse.h"
#include "sparse_ci/sq_operator_string.h"

namespace forte {

/// @brief A class to represent general second quantized operators
///
/// An operator is a linear combination of terms, where each term is a numerical factor
/// times a product of second quantized operators (a SQOperatorString object)
///
/// For example:
///   0.1 * [2a+ 0a-] - 0.5 * [2a+ 0a-] + ...
///           Term 0            Term 1
///
/// This class stores operators in each term in the following canonical form
///   a+_p1 a+_p2 ...  a+_P1 a+_P2 ...   ... a-_Q2 a-_Q1   ... a-_q2 a-_q1
///   alpha creation   beta creation    beta annihilation  alpha annihilation
///
/// with indices sorted as
///
///   (p1 < p2 < ...) (P1 < P2 < ...)  (... > Q2 > Q1) (... > q2 > q1)
///
class SparseOperator : public VectorSpace<SparseOperator, SQOperatorString, sparse_scalar_t,
                                          SQOperatorString::Hash> {
  public:
    SparseOperator() = default;
    using base_t =
        VectorSpace<SparseOperator, SQOperatorString, sparse_scalar_t, SQOperatorString::Hash>;
    using base_t::base_t; // Make the base class constructors visible

    /// @brief add a term to this operator
    /// @param str a string that defines the product of operators in the format [... q_2 q_1 q_0]
    /// @param coefficient a coefficient that multiplies the product of second quantized operators
    /// @param allow_reordering if true, the operator will be reordered to canonical form
    /// @details The operator is stored in canonical form
    ///
    ///     coefficient * [... q_2 q_1 q_0]
    ///
    /// where q_0, q_1, ... are second quantized operators. These operators are
    /// passed as string
    ///
    ///     '[... q_2 q_1 q_0]'
    ///
    /// where q_i = <orbital_i><spin_i><type_i> and the quantities in <> are
    ///
    ///     orbital_i: int
    ///     spin_i: 'a' (alpha) or 'b' (beta)
    ///     type_i: '+' (creation) or '-' (annihilation)
    ///
    /// For example, '[0a+ 1b+ 12b- 0a-]'
    ///
    void add_term_from_str(const std::string& str, sparse_scalar_t coefficient,
                           bool allow_reordering = false);

    /// @return a string representation of this operator
    std::vector<std::string> str() const;

    /// @return a latex representation of this operator
    std::string latex() const;
};

class NormalOrderedSparseOperator
    : public VectorSpace<NormalOrderedSparseOperator, SQOperatorString, sparse_scalar_t,
                         SQOperatorString::Hash> {
  public:
    NormalOrderedSparseOperator() = default;
    /// @brief  add a term to this operator
    /// @param str a string that defines the product of operators in the format [... q_2 q_1 q_0]
    /// @param coefficient a coefficient that multiplies the product of second quantized operators
    /// @param allow_reordering if true, the operator will be reordered to canonical form
    void add_term_from_str(std::string str, sparse_scalar_t coefficient,
                           bool allow_reordering = false);
};

class SparseOperatorList
    : public VectorSpaceList<SparseOperatorList, SQOperatorString, sparse_scalar_t> {
  public:
    SparseOperatorList() = default;

    void add_term_from_str(std::string str, sparse_scalar_t coefficient,
                           bool allow_reordering = false);

    /// @return a string representation of this operator
    std::vector<std::string> str() const;

    void add_term(const std::vector<std::tuple<bool, bool, int>>& op_list, double coefficient,
                  bool allow_reordering);

    SparseOperator to_operator() const;
};

/// @return The product of two second quantized operators
SparseOperator operator*(const SparseOperator& lhs, const SparseOperator& rhs);

/// @return The commutator of two second quantized operators
SparseOperator commutator(const SparseOperator& lhs, const SparseOperator& rhs);

/// @brief Evaluate a factorized similarity transformation of an operator generated by a generator T
/// @param O the operator to which the similarity transformations will be applied
/// @param T = (T1, T2, ...) the list of operators applied in the similarity transformations
/// @param reverse if true, apply the similarity transformations in reverse order
/// @param screen_threshold a threshold to select which elements of the operator will be applied
/// @details This function applies the similarity transformations in the order provided in the list
/// T O -> ... (1 - T2) (1 - T1) O (1 + T1) (1 + T2) ...
void fact_trans_lin(SparseOperator& O, const SparseOperatorList& T, bool reverse = false,
                    double screen_threshold = 1e-12);

/// @brief Evalate a factorized similarity transformation of an operator generated by antihermitian
/// operators
/// @param O the operator to which the similarity transformations will be applied
/// @param T  = (T1, T2, ...) the list of operators applied in the similarity transformations
/// @param reverse if true, apply the similarity transformations in reverse order
/// @param screen_threshold a threshold to select which elements of the operator will be applied
/// @details This function applies the similarity transformations in the order provided in the list
/// T O -> ... exp(-T2 + T2+) exp(-T1 + T1+) O exp(T1 - T1+) exp(T2 - T2+) ...
void fact_unitary_trans_antiherm(SparseOperator& O, const SparseOperatorList& T,
                                 bool reverse = false, double screen_threshold = 1e-12);

/// @brief Evalate the gradient of a factorized similarity transformation of an operator generated
/// by antihermitian operators
/// @param O the operator to which the similarity transformations will be applied
/// @param T  = (T1, T2, ...) the list of operators applied in the similarity transformations
/// @param n the index of the operator in the list T of which the gradient will be taken
/// @param reverse if true, apply the similarity transformations in reverse order
/// @param screen_threshold a threshold to select which elements of the operator will be applied
void fact_unitary_trans_antiherm_grad(SparseOperator& O, const SparseOperatorList& T, size_t n,
                                      bool reverse = false, double screen_threshold = 1e-12);

/// @brief Evalate a factorized similarity transformation of an operator generated by antihermitian
/// operators
/// @param O the operator to which the similarity transformations will be applied
/// @param T  = (T1, T2, ...) the list of operators applied in the similarity transformations
/// @param reverse if true, apply the similarity transformations in reverse order
/// @param screen_threshold a threshold to select which elements of the operator will be applied
/// @details This function applies the similarity transformations in the order provided in the list
/// T O -> ... exp(-i(T2 + T2+)) exp(-i(T1 + T1+)) O exp(i (T1 + T1+)) exp(i (T2 + T2+)) ...
void fact_unitary_trans_imagherm(SparseOperator& O, const SparseOperatorList& T,
                                 bool reverse = false, double screen_threshold = 1e-12);

} // namespace forte
