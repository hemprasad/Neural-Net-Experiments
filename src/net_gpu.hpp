#pragma once

#include "net.hpp"
#include "net_raw.hpp"
#include "net_raw_utils.hpp"

#include "net_gpu_impl.hpp"

template <typename activation, typename error>
inline void gpu_train_batch(FeedForward_Network<activation, error>& network,
    arma::Mat<float> inputs, arma::Mat<float> targets, int batch_size, float learning_rate = 0.8f, float momentum = 0.8f) {

  network.resize_activation(batch_size);
  Raw_FeedForward_Network<activation, error> raw_net = convert_to_raw(network);
  Raw_FeedForward_Network<activation, error> * d_network = network_to_gpu(raw_net);

  int batches_in_train = targets.n_rows/batch_size - 1;
  for (int i = 0; i < batches_in_train; ++i) {
    arma::Mat<float> input_slice = inputs.rows(i*batch_size, (i+1) * batch_size - 1);

    Raw_Matrix raw_input = to_raw(input_slice);
    Raw_Matrix * d_input = matrix_to_gpu(raw_input);
    int num_trials = input_slice.n_rows;

    calculate_activation(num_trials, network.layer_sizes, d_network, d_input);
    //TODO make this memory shared as to not realloc
    free_gpu_matrix(d_input);

    arma::Mat<float> targets_slice = targets.rows(i*batch_size, (i+1) * batch_size - 1);

    Raw_Matrix raw_targets = to_raw(targets_slice);
    Raw_Matrix * d_targets = matrix_to_gpu(raw_targets);

    backprop(num_trials, network.layer_sizes, d_network, d_targets, learning_rate, momentum);
    free_gpu_matrix(d_targets);
  }

  network_to_cpu_free(d_network, raw_net);
  update_from_raw(network, raw_net);

}

template <typename activation, typename error>
inline arma::Mat<float> gpu_predict(FeedForward_Network<activation, error>& network,
    arma::Mat<float> inputs) {
  network.resize_activation(inputs.n_rows);
  Raw_FeedForward_Network<activation, error> raw_net = convert_to_raw(network);
  Raw_FeedForward_Network<activation, error> * d_network = network_to_gpu(raw_net);
  Raw_Matrix raw_inputs = to_raw(inputs);
  Raw_Matrix * d_inputs = matrix_to_gpu(raw_inputs);

  int num_trials = inputs.n_rows;

  calculate_activation(num_trials, network.layer_sizes, d_network, d_inputs);
  free_gpu_matrix(d_inputs);

  network_to_cpu_free(d_network, raw_net);
  update_from_raw(network, raw_net);
  return network.activations.back();
}
