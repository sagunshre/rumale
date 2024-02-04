# frozen_string_literal: true

require 'spec_helper'

RSpec.describe Rumale::Manifold::LocalTangentSpaceAlignment do
  let!(:samples) { swiss_roll }
  let(:x) { samples[0] }
  let(:x_test) { samples[1] }
  let(:n_samples) { x.shape[0] }
  let(:n_features) { x.shape[1] }
  let(:n_test_samples) { x_test.shape[0] }
  let(:ltsa) { described_class.new(n_components: n_components, n_neighbors: 20) }
  let(:z) { ltsa.fit_transform(x) }
  let(:z_test) { ltsa.transform(x_test) }

  context 'when mapping to multi-dimensional subspace' do
    let(:n_components) { 2 }

    it 'maps high-dimensional data into low-dimensional data', :aggregate_failures do
      expect(z).to be_a(Numo::DFloat)
      expect(z).to be_contiguous
      expect(z.ndim).to eq(2)
      expect(z.shape[0]).to eq(n_samples)
      expect(z.shape[1]).to eq(n_components)
      expect(z_test).to be_a(Numo::DFloat)
      expect(z_test).to be_contiguous
      expect(z_test.ndim).to eq(2)
      expect(z_test.shape[0]).to eq(n_test_samples)
      expect(z_test.shape[1]).to eq(n_components)
      expect(ltsa.embedding).to be_a(Numo::DFloat)
      expect(ltsa.embedding).to be_contiguous
      expect(ltsa.embedding.ndim).to eq(2)
      expect(ltsa.embedding.shape[0]).to eq(n_samples)
      expect(ltsa.embedding.shape[1]).to eq(n_components)
    end
  end

  context 'when mapping to one-dimensional subspace' do
    let(:n_components) { 1 }

    it 'maps high-dimensional data into one-dimensional data', :aggregate_failures do
      expect(z).to be_a(Numo::DFloat)
      expect(z).to be_contiguous
      expect(z.ndim).to eq(1)
      expect(z.shape[0]).to eq(n_samples)
      expect(z_test).to be_a(Numo::DFloat)
      expect(z_test).to be_contiguous
      expect(z_test.ndim).to eq(1)
      expect(z_test.shape[0]).to eq(n_test_samples)
      expect(ltsa.embedding).to be_a(Numo::DFloat)
      expect(ltsa.embedding).to be_contiguous
      expect(ltsa.embedding.ndim).to eq(1)
      expect(ltsa.embedding.shape[0]).to eq(n_samples)
    end
  end
end
