/*
 * @BEGIN LICENSE
 *
 * Forte: an open-source plugin to Psi4 (https://github.com/psi4/psi4)
 * that implements a variety of quantum chemistry methods for strongly
 * correlated electrons.
 *
 * Copyright (c) 2012-2025 by its authors (see COPYING, COPYING.LESSER,
 * AUTHORS).
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

#include <string>
#include <tuple>
#include <vector>
#include <utility>
#include <unordered_set>

#include "ambit/tensor.h"

#include "ci_rdm/ci_rdms.h"
#include "base_classes/mo_space_info.h"
#include "helpers/helpers.h"
#include "integrals/integrals.h"
#include "base_classes/active_space_method.h"
#include "base_classes/rdms.h"
#include "sparse_ci/sparse_ci_solver.h"
#include "integrals/active_space_integrals.h"
#include "sparse_ci/determinant.h"

using d1 = std::vector<double>;
using d2 = std::vector<d1>;
using d3 = std::vector<d2>;
using d4 = std::vector<d3>;
using d5 = std::vector<d4>;
using d6 = std::vector<d5>;
using vecdet = std::vector<forte::Determinant>;

namespace forte {

class ForteOptions;
class SCFInfo;

/// Set the FCI_MO options
void set_FCI_MO_options(ForteOptions& foptions);

class FCI_MO : public ActiveSpaceMethod {

  public:
    using det_hash = std::unordered_map<Determinant, size_t, Determinant::Hash>;
    using det_hash_it = det_hash::iterator;
    /**
     * @brief FCI_MO Constructor
     * @param ref_wfn The reference wavefunction object
     * @param options PSI4 and FORTE options
     * @param ints ForteInegrals
     * @param mo_space_info MOSpaceInfo
     * @param fci_ints FCIInegrals
     */
    FCI_MO(StateInfo state, size_t nroot, std::shared_ptr<SCFInfo> scf_info,
           std::shared_ptr<ForteOptions> options, std::shared_ptr<MOSpaceInfo> mo_space_info,
           std::shared_ptr<ActiveSpaceIntegrals> as_ints);

    /**
     * @brief FCI_MO Constructor
     * @param ref_wfn The reference wavefunction object
     * @param options PSI4 and FORTE options
     * @param ints ForteInegrals
     * @param mo_space_info MOSpaceInfo
     */
    [[deprecated("Using a deprecated constructor that does not take state and nroot")]] FCI_MO(
        std::shared_ptr<SCFInfo> scf_info, std::shared_ptr<ForteOptions> options,
        std::shared_ptr<ForteIntegrals> ints, std::shared_ptr<MOSpaceInfo> mo_space_info);

    /**
     * @brief FCI_MO Constructor
     * @param ref_wfn The reference wavefunction object
     * @param options PSI4 and FORTE options
     * @param ints ForteInegrals
     * @param mo_space_info MOSpaceInfo
     * @param fci_ints FCIInegrals
     */
    [[deprecated("Using a deprecated constructor that does not take state and nroot")]] FCI_MO(
        std::shared_ptr<SCFInfo> scf_info, std::shared_ptr<ForteOptions> options,
        std::shared_ptr<ForteIntegrals> ints, std::shared_ptr<MOSpaceInfo> mo_space_info,
        std::shared_ptr<ActiveSpaceIntegrals> fci_ints);

    /// Destructor
    ~FCI_MO();

    /// Compute state-specific or state-averaged energy
    double compute_energy() override;

    /// Compute state-specific CASCI energy
    std::vector<double> compute_ss_energies();

    /// Compute the reduced density matrices up to a given particle rank (max_rdm_level)
    std::vector<std::shared_ptr<RDMs>> rdms(const std::vector<std::pair<size_t, size_t>>& root_list,
                                            int max_rdm_level, RDMsType rdm_type) override;

    /// Compute the generalized reduced density matrix of a given level
    void generalized_rdms(size_t root, const std::vector<double>& X, ambit::BlockedTensor& grdms,
                          bool c_right, int rdm_level, std::vector<std::string> spin) override;

    /// Add k-body contributions to the sigma vector
    ///    σ_I += h_{p1,p2,...}^{q1,q2,...} <Phi_I| a^+_p1 a^+_p2 .. a_q2 a_q1 |Phi_J> C_J
    /// @param root: the root number of the state
    /// @param h: the antisymmetrized k-body integrals
    /// @param block_label_to_factor: map from the block labels of integrals to its factors
    /// @param sigma: the sigma vector to be added
    void add_sigma_kbody(size_t root, ambit::BlockedTensor& h,
                         const std::map<std::string, double>& block_label_to_factor,
                         std::vector<double>& sigma) override;

    /// Compute generalized sigma vector
    ///     σ_I = <Phi_I| H |Phi_J> X_J where H is the active space Hamiltonian (fci_ints)
    /// @param x: the X vector to be contracted with H_IJ
    /// @param sigma: the sigma vector (will be zeroed first)
    void generalized_sigma(std::shared_ptr<psi::Vector> x,
                           std::shared_ptr<psi::Vector> sigma) override;

    /// Return the number of determinants
    size_t space_size() override { return determinant_.size(); }

    /// Returns the transition reduced density matrices between roots of different symmetry up to a
    /// given level (max_rdm_level)
    std::vector<std::shared_ptr<RDMs>>
    transition_rdms(const std::vector<std::pair<size_t, size_t>>& root_list,
                    std::shared_ptr<ActiveSpaceMethod> method2, int max_rdm_level,
                    RDMsType rdm_type) override;

    /// Compute the overlap of two wave functions acted by complementary operators
    /// Return a map from state to roots of values
    /// Computes the overlap of \sum_{p} \sum_{σ} <Ψ| h^+_{pσ} (v) h_{pσ} (t) |Ψ>, where
    /// h_{pσ} (t) = \sum_{uvw} t^{uv}_{pw} \sum_{τ} w^+_{τ} v_{τ} u_{σ}
    /// Useful to get the 3-RDM contribution of fully contracted term of two 2-body operators:
    /// \sum_{puvwxyzστθ} v_{pwxy} t_{uvpz} <Ψ| xσ^+ yτ^+ wτ zθ^+ vθ uσ |Ψ>
    std::vector<double> compute_complementary_H2caa_overlap(const std::vector<size_t>& roots,
                                                            ambit::Tensor Tbra,
                                                            ambit::Tensor Tket) override;
    std::vector<double>
    compute_complementary_H2caa_overlap_mo_driven(const std::vector<size_t>& roots,
                                                  ambit::Tensor Tbra, ambit::Tensor Tket);
    std::vector<double>
    compute_complementary_H2caa_overlap_ci_driven(const std::vector<size_t>& roots,
                                                  ambit::Tensor Tbra, ambit::Tensor Tket);

    [[deprecated]] std::vector<std::shared_ptr<RDMs>>
    reference(const std::vector<std::pair<size_t, size_t>>& root_list, int max_rdm_level);

    std::shared_ptr<RDMs> reference(int max_rdm_level) {
        std::vector<std::pair<size_t, size_t>> roots;
        roots.push_back(std::make_pair(0, 0));
        return reference(roots, max_rdm_level)[0];
    }

    void set_options(std::shared_ptr<ForteOptions>) override {} // TODO implement

    /// Return the CI wave functions for current state symmetry
    std::shared_ptr<psi::Matrix> ci_wave_functions() override;

    /// Compute densities or transition densities
    /// root1, root2 -- the ket and bra roots of p_space and eigen
    /// multi_state -- grab p_spaces_ and eigens_ if true, otherwise p_space_ and eigen_
    /// entry -- symmetry entry of p_spaces_ and eigens_ (same entry as sa_info_)
    /// max_level -- max RDM level to be computed
    /// do_cumulant -- returned RDMs is filled by cumulants (not RDMs) if true
    std::shared_ptr<RDMs> transition_reference(int root1, int root2, bool multi_state,
                                               int entry = 0, int max_level = 3,
                                               bool do_cumulant = false, bool disk = true);

    /// Density files
    std::vector<std::string> density_filenames_generator(int rdm_level, int irrep, int multi,
                                                         int root1, int root2);
    bool check_density_files_fcimo(int rdm_level, int irrep, int multi, int root1, int root2);
    void remove_density_files_fcimo(int rdm_level, int irrep, int multi, int root1, int root2);

    /// Generate density file names at a certain RDM level
    std::vector<std::string> generate_rdm_file_names(int rdm_level, int root1, int root2,
                                                     const StateInfo& state2);
    /// Check if density files for a given RDM level already exist
    bool check_density_files(int rdm_level, int root1, int root2, const StateInfo& state2);
    /// Remove density files for a given RDM level
    void remove_density_files(int rdm_level, int root1, int root2, const StateInfo& state2);

    /// Compute dipole moments with DSRG transformed MO dipole integrals
    /// This function is used for reference relaxation and SA-MRDSRG
    /// This function should be in RUN_DSRG
    std::map<std::string, std::vector<double>>
    compute_ref_relaxed_dm(const std::vector<double>& dm0, std::vector<ambit::BlockedTensor>& dm1,
                           std::vector<ambit::BlockedTensor>& dm2);
    std::map<std::string, std::vector<double>>
    compute_ref_relaxed_dm(const std::vector<double>& dm0, std::vector<ambit::BlockedTensor>& dm1,
                           std::vector<ambit::BlockedTensor>& dm2,
                           std::vector<ambit::BlockedTensor>& dm3);

    /// Compute oscillator strengths using DSRG transformed MO dipole integrals
    /// This function is used for SA-MRDSRG
    /// This function should be in RUN_DSRG
    std::map<std::string, std::vector<double>>
    compute_ref_relaxed_osc(std::vector<ambit::BlockedTensor>& dm1,
                            std::vector<ambit::BlockedTensor>& dm2);
    std::map<std::string, std::vector<double>>
    compute_ref_relaxed_osc(std::vector<ambit::BlockedTensor>& dm1,
                            std::vector<ambit::BlockedTensor>& dm2,
                            std::vector<ambit::BlockedTensor>& dm3);

    /**
     * @brief Rotate the SA references such that <M|F|N> is diagonal
     * @param irrep The irrep of states M and N (same irrep)
     */
    void xms_rotate_civecs();

    /// Set if safe to read densities from files
    void set_safe_to_read_density_files(bool safe) { safe_to_read_density_files_ = safe; }

    /// Set fci_int_ pointer
    void set_fci_int(std::shared_ptr<ActiveSpaceIntegrals> fci_ints) { fci_ints_ = fci_ints; }

    /// Set multiplicity
    void set_multiplicity(int multiplicity) { multi_ = multiplicity; }

    /// Set symmetry of the root
    void set_root_sym(int root_sym) { root_sym_ = root_sym; }

    /// Set number of roots
    void set_nroots(int nroot) { nroot_ = nroot; }

    /// Set if localize orbitals
    void set_localize_actv(bool localize) { localize_actv_ = localize; }

    /// Set projected roots
    void project_roots(std::vector<std::vector<std::pair<size_t, double>>>& projected) {
        projected_roots_ = projected;
    }

    /// Set initial guess
    void set_initial_guess(const std::vector<std::vector<std::pair<size_t, double>>>& guess) {
        initial_guess_ = guess;
    }

    /// Set SA infomation
    void set_sa_info(const std::vector<std::tuple<int, int, int, std::vector<double>>>& info);

    /// Set state-averaged eigen values and vectors
    void set_eigens(
        const std::vector<std::vector<std::pair<std::shared_ptr<psi::Vector>, double>>>& eigens);

    /// Return fci_int_ pointer
    std::shared_ptr<ActiveSpaceIntegrals> fci_ints() { return fci_ints_; }

    /// Return the vector of determinants
    const vecdet& p_space() const { return determinant_; }

    /// Return P spaces for states with different symmetry
    std::vector<vecdet> p_spaces() { return p_spaces_; }

    /// Return the orbital extents of the current state
    std::vector<std::vector<std::vector<double>>> orb_extents() {
        return compute_orbital_extents();
    }

    /// Return the vector of eigen vectors and eigen values
    std::vector<std::pair<std::shared_ptr<psi::Vector>, double>> const eigen() { return eigen_; }

    /// Return the vector of eigen vectors and eigen values (used in state-average computation)
    std::vector<std::vector<std::pair<std::shared_ptr<psi::Vector>, double>>> const eigens() {
        return eigens_;
    }

    /// Return a vector of dominant determinant for each root
    std::vector<Determinant> dominant_dets() { return dominant_dets_; }

    /// Return indices (relative to active, not absolute) of active occupied orbitals
    std::vector<size_t> actv_occ() { return actv_hole_mos_; }

    /// Return indices (relative to active, not absolute) of active virtual orbitals
    std::vector<size_t> actv_uocc() { return actv_part_mos_; }

    /// Return the psi::Dimension of active occupied orbitals
    psi::Dimension actv_docc() { return actv_hole_dim_; }

    /// Return the psi::Dimension of active virtual orbitals
    psi::Dimension actv_virt() { return actv_part_dim_; }

    /// Return the T1 percentage in CISD computations
    std::vector<double> compute_T1_percentage();

    /// Return the parsed state-averaged info
    std::vector<std::tuple<int, int, int, std::vector<double>>> sa_info() { return sa_info_; }

    /// Return the eigen vector in ambit Tensor format
    std::vector<ambit::Tensor> eigenvectors() override;

    /// Return the size of determinants
    size_t det_size();

  protected:
    /// Basic Preparation
    void startup();
    void read_options();
    void print_options();
    void cleanup();

    /// Integrals
    std::shared_ptr<ForteIntegrals> integral_;
    std::string int_type_;
    std::shared_ptr<ActiveSpaceIntegrals> fci_ints_;

    /// Reference Type
    std::string ref_type_;

    /// MO space info
    std::shared_ptr<MOSpaceInfo> mo_space_info_;

    /// SCF info
    std::shared_ptr<SCFInfo> scf_info_;

    /// ForteOptions
    std::shared_ptr<ForteOptions> options_;

    /// Print Levels
    int print_;

    /// Nucear Repulsion Energy
    double e_nuc_;

    /// Convergence
    double econv_;

    /// Convergence
    double rconv_;

    /// Multiplicity
    int multi_;
    int twice_ms_;
    std::vector<std::string> multi_symbols_;

    /// Symmetry
    int nirrep_;                // number of irrep
    int root_sym_;              // root
    std::vector<int> sym_actv_; // active MOs
    std::vector<std::string> irrep_symbols_;

    /// Molecular Orbitals
    size_t nmo_; // total MOs
    psi::Dimension nmopi_;
    size_t ncmo_; // correlated MOs
    psi::Dimension ncmopi_;
    psi::Dimension frzc_dim_; // frozen core
    psi::Dimension frzv_dim_; // frozen virtual
    size_t nfrzc_;
    size_t nfrzv_;
    psi::Dimension core_dim_; // core MOs
    size_t ncore_;
    std::vector<size_t> core_mos_;
    psi::Dimension actv_dim_; // active MOs
    size_t nactv_;
    std::vector<size_t> actv_mos_;
    size_t nvirt_; // virtual MOs
    psi::Dimension virt_dim_;
    std::vector<size_t> virt_mos_;
    size_t nhole_; // hole MOs
    std::vector<size_t> hole_mos_;
    size_t npart_; // particle MOs
    std::vector<size_t> part_mos_;
    psi::Dimension actv_hole_dim_; // active hole for incomplete active space
    std::vector<size_t> actv_hole_mos_;
    psi::Dimension actv_part_dim_; // active particle for incomplete active space
    std::vector<size_t> actv_part_mos_;

    /// Compute IP or EA
    std::string ipea_;

    /// Number of Alpha and Beta Electrons
    int nalfa_;
    int nbeta_;

    /// Active Space Type: CAS, CIS, CISD
    std::string actv_space_type_;

    /// Form Determiants Space
    void form_p_space();

    /// Determinants
    void form_det();
    void form_det_cis();
    void form_det_cisd();
    vecdet determinant_;
    std::vector<Determinant> dominant_dets_;
    std::vector<vecdet> p_spaces_;

    /// Determinants in hash vector form
    DeterminantHashVec det_hash_vec_;

    /// SigmaVector object
    std::shared_ptr<SigmaVector> sigma_vector_;

    /// Size of Singles Determinants
    size_t singles_size_;

    /// Orbital Strings
    std::vector<std::vector<std::vector<bool>>> Form_String(const int& active_elec,
                                                            const bool& print = false);
    std::vector<bool> Form_String_Ref(const bool& print = false);
    std::vector<std::vector<std::vector<bool>>>
    Form_String_Singles(const std::vector<bool>& ref_string, const bool& print = false);
    std::vector<std::vector<std::vector<bool>>>
    Form_String_Doubles(const std::vector<bool>& ref_string, const bool& print = false);
    std::vector<std::vector<std::vector<bool>>> Form_String_IP(const std::vector<bool>& ref_string,
                                                               const bool& print = false);
    std::vector<std::vector<std::vector<bool>>> Form_String_EA(const std::vector<bool>& ref_string,
                                                               const bool& print = false);

    /// State Average Information (tuple of irrep, multi, nstates, weights)
    std::vector<std::tuple<int, int, int, std::vector<double>>> sa_info_;

    /// Roots to be projected out in the diagonalization
    std::vector<std::vector<std::pair<size_t, double>>> projected_roots_;

    /// Initial guess vector
    std::vector<std::vector<std::pair<size_t, double>>> initial_guess_;

    /// Eigen Values and Eigen Vectors of Certain Symmetry
    std::vector<std::pair<std::shared_ptr<psi::Vector>, double>> eigen_;
    /// A List of Eigen Values and Vectors for State Average
    std::vector<std::vector<std::pair<std::shared_ptr<psi::Vector>, double>>> eigens_;
    /// The algorithm for diagonalization
    std::string diag_algorithm_;

    /// Diagonalize the Hamiltonian
    void Diagonalize_H(const vecdet& P_space, const int& multi, const int& nroot,
                       std::vector<std::pair<std::shared_ptr<psi::Vector>, double>>& eigen);
    /// Diagonalize the Hamiltonian without the HF determinant
    void Diagonalize_H_noHF(const vecdet& p_space, const int& multi, const int& nroot,
                            std::vector<std::pair<std::shared_ptr<psi::Vector>, double>>& eigen);

    /// Print the CI Vectors and Configurations (figure out the dominant determinants)
    void print_CI(const int& nroot, const double& CI_threshold,
                  const std::vector<std::pair<std::shared_ptr<psi::Vector>, double>>& eigen,
                  const vecdet& det);

    /// Density Matrix
    ambit::Tensor L1a_; // only in active
    ambit::Tensor L1b_; // only in active

    /// 2-Body Density Cumulant
    ambit::Tensor L2aa_;
    ambit::Tensor L2ab_;
    ambit::Tensor L2bb_;

    /// 3-Body Density Cumulant
    ambit::Tensor L3aaa_;
    ambit::Tensor L3aab_;
    ambit::Tensor L3abb_;
    ambit::Tensor L3bbb_;

    /// File Names of Densities Stored on Disk
    std::unordered_set<std::string> density_files_;
    bool safe_to_read_density_files_ = false;
    void clean_all_density_files();

    /// Prepare eigen vectors for RDM or TRDM (within current symmetry) computations
    std::shared_ptr<psi::Matrix> prepare_for_rdm();

    /// Prepare determinant space and eigen vectors for TRDM computations between different
    /// symmetries
    std::pair<std::shared_ptr<vecdet>, std::shared_ptr<psi::Matrix>>
    prepare_for_trans_rdm(std::shared_ptr<FCI_MO> method2);

    /// Compute reduced density matricies for given determinant space and eigen vectors
    [[deprecated("Soon will be using StateInfo based rdm function")]] std::vector<ambit::Tensor>
    compute_n_rdm(const vecdet& p_space, std::shared_ptr<psi::Matrix> evecs, int rdm_level,
                  int root1, int root2, int irrep, int multi, bool disk);

    std::vector<ambit::Tensor> compute_n_rdm(const vecdet& p_space,
                                             std::shared_ptr<psi::Matrix> evecs, int rdm_level,
                                             int root1, int root2, const StateInfo& state2,
                                             bool disk);

    ambit::Tensor compute_n_rdm_sf(const vecdet& p_space, std::shared_ptr<psi::Matrix> evecs,
                                   int rdm_level, int root1, int root2, const StateInfo& state2);

    /// Rotate the given CI vectors by XMS
    std::shared_ptr<psi::Matrix> xms_rotate_this_civecs(const det_vec& p_space,
                                                        std::shared_ptr<psi::Matrix> civecs,
                                                        ambit::Tensor Fa, ambit::Tensor Fb);

    /// Reference Energy
    double Eref_;

    /// Compute 2- and 3-cumulants
    void compute_ref(const int& level, size_t root1, size_t root2);

    /// Build a map from (N-1)-electron string to determinants
    std::vector<std::vector<std::pair<int, size_t>>> am1_string_to_dets_;
    std::vector<std::vector<std::pair<int, size_t>>> bm1_string_to_dets_;
    void build_nm1_string_dets_map();

    /// 3 to 1 determinants: z^+ v u |I>
    det_hash dets321_a_; // u: alpha
    det_hash dets321_b_; // u: beta

    /// Build 3 to 1 lists: z^+ v u |I>
    void build_dets321();
    bool built_dets321_ = false;

    /// 3 to 1 lists: |J> = z^+ v u |I>
    /// J to vector of tuples (I, u, v, z, sign)
    std::vector<std::vector<std::tuple<size_t, unsigned char, unsigned char, unsigned char, bool>>>
        list321_aaa_;
    std::vector<std::vector<std::tuple<size_t, unsigned char, unsigned char, unsigned char, bool>>>
        list321_abb_;
    std::vector<std::vector<std::tuple<size_t, unsigned char, unsigned char, unsigned char, bool>>>
        list321_baa_;
    std::vector<std::vector<std::tuple<size_t, unsigned char, unsigned char, unsigned char, bool>>>
        list321_bbb_;

    /// Build 3 to 1 lists: |J> = z^+ v u |I>
    void build_lists321();
    bool built_lists321_ = false;

    /// Orbital Extents
    /// returns a vector of irrep by # active orbitals in current irrep
    /// by orbital extents {xx, yy, zz}
    d3 compute_orbital_extents();
    size_t idx_diffused_;
    std::vector<size_t> diffused_orbs_;

    /// Compute permanent dipole moments
    void compute_permanent_dipole();

    /// Reformat 1RDM from nactv x nactv vector to N x N std::shared_ptr<psi::Matrix>
    std::shared_ptr<psi::Matrix> reformat_1rdm(const std::string& name,
                                               const std::vector<double>& data, bool TrD);

    /// Transition dipoles
    std::map<std::string, std::vector<double>> trans_dipole_;
    /// Compute transition dipole of same symmetry
    void compute_transition_dipole();
    /// Compute oscillator strength of same symmetry
    void compute_oscillator_strength();

    /// Compute dipole (or transition dipole) using DSRG transformed MO dipole integrals (dm)
    /// and densities (or transition densities, D)
    double ref_relaxed_dm_helper(const double& dm0, ambit::BlockedTensor& dm1,
                                 ambit::BlockedTensor& dm2, ambit::BlockedTensor& D1,
                                 ambit::BlockedTensor& D2);
    double ref_relaxed_dm_helper(const double& dm0, ambit::BlockedTensor& dm1,
                                 ambit::BlockedTensor& dm2, ambit::BlockedTensor& dm3,
                                 ambit::BlockedTensor& D1, ambit::BlockedTensor& D2,
                                 ambit::BlockedTensor& D3);

    /// Compute RDMs at given order and put into BlockedTensor format
    ambit::BlockedTensor compute_n_rdm(CI_RDMS& cirdm, const int& order);

    /// Localize active orbitals
    bool localize_actv_;

    /// Print Determinants
    void print_det(const vecdet& dets);

    /// Print occupations of strings
    void
    print_occupation_strings_perirrep(std::string name,
                                      const std::vector<std::vector<std::vector<bool>>>& string);
};
} // namespace forte
