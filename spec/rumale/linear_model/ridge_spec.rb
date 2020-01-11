# frozen_string_literal: true

require 'spec_helper'

RSpec.describe Rumale::LinearModel::Ridge do
  let(:x) { two_clusters_dataset[0] }
  let(:single_target) { x.dot(Numo::DFloat[1.0, 2.0]) }
  let(:multi_target) { x.dot(Numo::DFloat[[1.0, 2.0], [2.0, 1.0]]) }
  let(:n_samples) { x.shape[0] }
  let(:n_features) { x.shape[1] }
  let(:n_outputs) { multi_target.shape[1] }
  let(:fit_bias) { false }
  let(:solver) { 'sgd' }
  let(:n_jobs) { nil }
  let(:estimator) { described_class.new(reg_param: 0.1, fit_bias: fit_bias, solver: solver, n_jobs: n_jobs, random_seed: 1).fit(x, y) }
  let(:predicted) { estimator.predict(x) }
  let(:score) { estimator.score(x, y) }
  let(:copied) { Marshal.load(Marshal.dump(estimator)) }

  shared_examples 'single regression' do
    let(:y) { single_target }

    it 'learns the model for single regression problem.', :aggregate_failures do
      expect(estimator.weight_vec.class).to eq(Numo::DFloat)
      expect(estimator.weight_vec.ndim).to eq(1)
      expect(estimator.weight_vec.shape[0]).to eq(n_features)
      expect(estimator.bias_term).to be_zero
      expect(predicted.class).to eq(Numo::DFloat)
      expect(predicted.ndim).to eq(1)
      expect(predicted.shape[0]).to eq(n_samples)
      expect(score).to be_within(0.01).of(1.0)
    end

    it 'dumps and restores itself using Marshal module.', :aggregate_failures do
      expect(copied.class).to eq(estimator.class)
      expect(copied.params).to eq(estimator.params)
      expect(copied.weight_vec).to eq(estimator.weight_vec)
      expect(copied.bias_term).to eq(estimator.bias_term)
      expect(copied.rng).to eq(estimator.rng)
      expect(copied.score(x, y)).to eq(score)
      expect(copied.instance_variable_get(:@penalty_type)).to eq('l2')
      expect(copied.instance_variable_get(:@loss_func).class).to eq(Rumale::LinearModel::Loss::MeanSquaredError)
    end
  end

  shared_examples 'single regression with bias' do
    let(:y) { single_target }
    let(:fit_bias) { true }

    it 'learns the model for single regression problem with bias term.', :aggregate_failures do
      expect(estimator.weight_vec.ndim).to eq(1)
      expect(estimator.weight_vec.shape[0]).to eq(n_features)
      expect(estimator.bias_term).not_to be_zero
      expect(score).to be_within(0.01).of(1.0)
    end
  end

  shared_examples 'multiple regression' do
    let(:y) { multi_target }

    it 'learns the model for multiple-regression problems.', :aggregate_failures do
      expect(estimator.weight_vec.class).to eq(Numo::DFloat)
      expect(estimator.weight_vec.ndim).to eq(2)
      expect(estimator.weight_vec.shape[0]).to eq(n_features)
      expect(estimator.weight_vec.shape[1]).to eq(n_outputs)
      expect(predicted.class).to eq(Numo::DFloat)
      expect(predicted.ndim).to eq(2)
      expect(predicted.shape[0]).to eq(n_samples)
      expect(predicted.shape[1]).to eq(n_outputs)
      expect(score).to be_within(0.01).of(1.0)
    end
  end

  shared_examples 'multiple regression with bias' do
    let(:y) { multi_target }
    let(:fit_bias) { true }

    it 'learns the model for single regression problem with bias term.', :aggregate_failures do
      expect(estimator.weight_vec.class).to eq(Numo::DFloat)
      expect(estimator.weight_vec.ndim).to eq(2)
      expect(estimator.weight_vec.shape[0]).to eq(n_features)
      expect(estimator.weight_vec.shape[1]).to eq(n_outputs)
      expect(estimator.bias_term.class).to eq(Numo::DFloat)
      expect(estimator.bias_term.ndim).to eq(1)
      expect(estimator.bias_term.shape[0]).to eq(n_outputs)
      expect(Math.sqrt((estimator.bias_term**2).sum)).not_to be_zero
      expect(score).to be_within(0.01).of(1.0)
    end
  end

  shared_examples 'multiple regression with parallel' do
    let(:y) { multi_target }
    let(:n_jobs) { -1 }

    it 'learns the model for multiple-regression problems.', :aggregate_failures do
      expect(estimator.weight_vec.class).to eq(Numo::DFloat)
      expect(estimator.weight_vec.ndim).to eq(2)
      expect(estimator.weight_vec.shape[0]).to eq(n_features)
      expect(estimator.weight_vec.shape[1]).to eq(n_outputs)
      expect(predicted.class).to eq(Numo::DFloat)
      expect(predicted.ndim).to eq(2)
      expect(predicted.shape[0]).to eq(n_samples)
      expect(predicted.shape[1]).to eq(n_outputs)
      expect(score).to be_within(0.01).of(1.0)
    end
  end

  context 'when solver is stochastic gradient descent' do
    let(:solver) { 'sgd' }

    it_behaves_like 'single regression'
    it_behaves_like 'single regression with bias'
    it_behaves_like 'multiple regression'
    it_behaves_like 'multiple regression with bias'
    it_behaves_like 'multiple regression with parallel'
  end

  context 'when solver is singular value decomposition' do
    let(:solver) { 'svd' }

    it_behaves_like 'single regression'
    it_behaves_like 'single regression with bias'
    it_behaves_like 'multiple regression with bias'
    it_behaves_like 'multiple regression'
  end
end
