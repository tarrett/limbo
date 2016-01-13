#ifndef LIMBO_BAYES_OPT_BOPTIMIZER_HPP
#define LIMBO_BAYES_OPT_BOPTIMIZER_HPP

#include <iostream>
#include <algorithm>
#include <iterator>

#include <boost/parameter/aux_/void.hpp>

#include <Eigen/Core>

#include <limbo/tools/macros.hpp>
#include <limbo/bayes_opt/bo_base.hpp>

namespace limbo {
    namespace defaults {
        struct bayes_opt_boptimizer {
            BO_PARAM(double, noise, 1e-6);
        };
    }
    namespace bayes_opt {

        template <class Params, class A1 = boost::parameter::void_,
            class A2 = boost::parameter::void_, class A3 = boost::parameter::void_,
            class A4 = boost::parameter::void_, class A5 = boost::parameter::void_,
            class A6 = boost::parameter::void_>
        class BOptimizer : public BoBase<Params, A1, A2, A3, A4, A5, A6> {
        public:
            typedef BoBase<Params, A1, A2, A3, A4, A5, A6> base_t;
            typedef typename base_t::model_t model_t;
            typedef typename base_t::acquisition_function_t acquisition_function_t;
            typedef typename base_t::acqui_optimizer_t acqui_optimizer_t;

            template <typename StateFunction, typename AggregatorFunction = FirstElem>
            void optimize(const StateFunction& sfun, const AggregatorFunction& afun = AggregatorFunction(), bool reset = true)
            {
                _model = model_t(StateFunction::dim_in, StateFunction::dim_out);
                this->_init(sfun, afun, reset);

                if (!this->_observations.empty())
                    _model.compute(this->_samples, this->_observations, Params::bayes_opt_boptimizer::noise(), this->_bl_samples);

                acqui_optimizer_t acqui_optimizer;

                while (this->_samples.size() == 0 || !this->_stop(*this, afun)) {
                    acquisition_function_t acqui(_model, this->_current_iteration);

                    // we do not have gradient in our current acquisition function
                    auto acqui_optimization =
                        [&](const Eigen::VectorXd& x, bool g) { return opt::no_grad(acqui(x, afun)); };
                    Eigen::VectorXd starting_point = tools::rand_vec(StateFunction::dim_in);
                    Eigen::VectorXd new_sample = acqui_optimizer(acqui_optimization, starting_point, true);
                    bool blacklisted = false;
                    try {
                        this->add_new_sample(new_sample, sfun(new_sample));
                    }
                    catch (...) {
                        this->add_new_bl_sample(new_sample);
                        blacklisted = true;
                    }

                    this->_update_stats(*this, afun, blacklisted);

                    if (!this->_observations.empty())
                        _model.compute(this->_samples, this->_observations, Params::bayes_opt_boptimizer::noise(), this->_bl_samples);

                    this->_current_iteration++;
                    this->_total_iterations++;
                }
            }

            template <typename AggregatorFunction = FirstElem>
            const Eigen::VectorXd& best_observation(const AggregatorFunction& afun = AggregatorFunction()) const
            {
                auto rewards = std::vector<double>(this->_observations.size());
                std::transform(this->_observations.begin(), this->_observations.end(), rewards.begin(), afun);
                auto max_e = std::max_element(rewards.begin(), rewards.end());
                return this->_observations[std::distance(rewards.begin(), max_e)];
            }

            template <typename AggregatorFunction = FirstElem>
            const Eigen::VectorXd& best_sample(const AggregatorFunction& afun = AggregatorFunction()) const
            {
                auto rewards = std::vector<double>(this->_observations.size());
                std::transform(this->_observations.begin(), this->_observations.end(), rewards.begin(), afun);
                auto max_e = std::max_element(rewards.begin(), rewards.end());
                return this->_samples[std::distance(rewards.begin(), max_e)];
            }

            const model_t& model() const { return _model; }

        protected:
            model_t _model;
        };
    }
}

#endif
