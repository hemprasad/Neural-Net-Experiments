#include "net.hpp"
#include "net_gpu.hpp"
#include "net_cpu.hpp"
#include "net_raw_utils.hpp"

#include <gtest/gtest.h>


TEST(gpu_utils, raw_to_gpu_and_back) {
  Raw_Matrix r;
  r.n_rows = 10;
  r.n_cols = 30;
  r.data = new float[100];
  for (int i=0; i < 100; i++) {
    r.data[i] = i;
  }

  Raw_Matrix * d_r = matrix_to_gpu(r);
  Raw_Matrix back_r =  matrix_to_cpu(d_r);

  ASSERT_EQ(back_r.n_cols, r.n_cols);
  ASSERT_EQ(back_r.n_rows, r.n_rows);
  for (int i=0; i < 100; i++) {
    ASSERT_EQ(back_r.data[i], r.data[i]);
  }
  delete r.data;
}


TEST(gpu_utils, raw_net_to_gpu_and_back) {
  FeedForward_Network<> f({5, 10, 4});
  f.resize_activation(5);
  randomize(f);
  for (int i = 0; i < 4; ++i) {
    f.activations[0][i] = i*1;
    f.activations[1][i] = i*2;
    f.activations[2][i] = i*3;
    f.deltas[0][i] = i * 4;
    f.deltas[1][i] = i * 5;
  }

  Raw_FeedForward_Network<> raw_net = convert_to_raw(f);

  Raw_FeedForward_Network<> * d_net = network_to_gpu(raw_net);

  int input_size = raw_net.layer_sizes[0];
  int output_size = raw_net.layer_sizes[1];
  int hidden_size = raw_net.layer_sizes[2];
  float inputToHidden4 = raw_net.weights[0].data[4];
  float inputToHidden5 = raw_net.weights[0].data[5];

  float hiddenToOutput4 = raw_net.weights[1].data[4];
  float hiddenToOutput5 = raw_net.weights[1].data[5];

  float hiddenToOutput_n_rows = raw_net.weights[1].n_rows;
  float hiddenToOutput_n_cols = raw_net.weights[1].n_cols;
  float inputToHidden_n_rows = raw_net.weights[0].n_rows;
  float inputToHidden_n_cols = raw_net.weights[0].n_cols;

  //Fudge the data to ensure its properly reset
  raw_net.layer_sizes[0] = 1;
  raw_net.layer_sizes[1] = 2;
  raw_net.layer_sizes[2] = 1;

  raw_net.weights[0].data[4] = -1;
  raw_net.weights[0].data[5] = -1;

  raw_net.weights[1].data[4] = -1;
  raw_net.weights[1].data[5] = -1;

  for (int i = 0; i < 4; ++i) {
    raw_net.activations[0].data[i] = -1;
    raw_net.activations[1].data[i] = -1;
    raw_net.activations[2].data[i] = -1;
    raw_net.deltas[0].data[i] = -1;
    raw_net.deltas[1].data[i] = -1;
  }

  //re copy back from gpu
  network_to_cpu_free(d_net, raw_net);

  ASSERT_EQ(raw_net.layer_sizes[0], input_size);
  ASSERT_EQ(raw_net.layer_sizes[1], output_size);
  ASSERT_EQ(raw_net.layer_sizes[2], hidden_size);

  ASSERT_EQ(raw_net.weights[0].data[4], inputToHidden4);
  ASSERT_EQ(raw_net.weights[0].data[5], inputToHidden5);

  ASSERT_EQ(raw_net.weights[1].data[4], hiddenToOutput4);
  ASSERT_EQ(raw_net.weights[1].data[5], hiddenToOutput5);

  ASSERT_EQ(raw_net.weights[1].n_rows, hiddenToOutput_n_rows);
  ASSERT_EQ(raw_net.weights[1].n_cols, hiddenToOutput_n_cols);

  ASSERT_EQ(raw_net.weights[0].n_rows, inputToHidden_n_rows);
  ASSERT_EQ(raw_net.weights[0].n_cols, inputToHidden_n_cols);

  for (int i = 0; i < 4; ++i) {
    ASSERT_FLOAT_EQ(raw_net.activations[0].data[i], i*1.f);
    ASSERT_FLOAT_EQ(raw_net.activations[1].data[i], i*2.f);
    ASSERT_FLOAT_EQ(raw_net.activations[2].data[i], i*3.f);
    ASSERT_FLOAT_EQ(raw_net.deltas[0].data[i], i*4.f);
    ASSERT_FLOAT_EQ(raw_net.deltas[1].data[i], i*5.f);
  }
}


TEST(net_gpu, calculate_activation) {
  FeedForward_Network<> f({2, 2, 1});
  f.resize_activation(3);
  f.weights[0].at(0,0) = -1;
  f.weights[0].at(1,0) = -.1;
  f.weights[0].at(0,1) = .1;
  f.weights[0].at(1,1) = 1;

  f.weights[1].at(0,0) = 1;
  f.weights[1].at(1,0) = 0;

  Raw_FeedForward_Network<> raw_net = convert_to_raw(f);
  Raw_FeedForward_Network<> * d_net = network_to_gpu(raw_net);

  arma::Mat<float> inputs(3, 2);
  inputs.at(0,0) = 0;
  inputs.at(0,1) = 0;

  inputs.at(1,0) = 1;
  inputs.at(1,1) = 0;

  inputs.at(2,0) = 0;
  inputs.at(2,1) = 1;

  Raw_Matrix raw_inputs = to_raw(inputs);
  Raw_Matrix * d_inputs = matrix_to_gpu(raw_inputs);

  int num_trials = 3;
  calculate_activation(num_trials, f.layer_sizes, d_net, d_inputs);

  network_to_cpu_free(d_net, raw_net);
  //input activations
  ASSERT_EQ(raw_net.activations[0].n_rows, 3);
  ASSERT_EQ(raw_net.activations[0].n_cols, 2);
  ASSERT_FLOAT_EQ(raw_net.activations[0].at(0,0), 0);
  ASSERT_FLOAT_EQ(raw_net.activations[0].at(1,0), 1);
  ASSERT_FLOAT_EQ(raw_net.activations[0].at(2,1), 1);
  ASSERT_FLOAT_EQ(raw_net.activations[0].at(2,0), 0);

  //hidden activationss (calculated with sigmoid activations
  ASSERT_EQ(raw_net.activations[1].n_rows, 3);
  ASSERT_EQ(raw_net.activations[1].n_cols, 2);

  ASSERT_NEAR(raw_net.activations[1].at(0,0), 0.5, .1);
  ASSERT_NEAR(raw_net.activations[1].at(0,1), 0.5, .1);

  ASSERT_NEAR(raw_net.activations[1].at(1,0), 0.27, .1);
  ASSERT_NEAR(raw_net.activations[1].at(1,1), .52, .1);

  ASSERT_NEAR(raw_net.activations[1].at(2,0), 0.4750, .1);
  ASSERT_NEAR(raw_net.activations[1].at(2,1), 0.7311, .1);

  //output activationss (calculated with signmoid)
  ASSERT_EQ(raw_net.activations[2].n_rows, 3);
  ASSERT_EQ(raw_net.activations[2].n_cols, 1);

  ASSERT_NEAR(raw_net.activations[2].at(0,0), 0.62, .1);
  ASSERT_NEAR(raw_net.activations[2].at(1,0), 0.57, .1);
  ASSERT_NEAR(raw_net.activations[2].at(2,0), 0.62, .1);
}

TEST(net_gpu, back_prop) {
  FeedForward_Network<> f({2, 2, 1});
  f.resize_activation(3);
  f.weights[0].at(0,0) = -1;
  f.weights[0].at(1,0) = -.1;
  f.weights[0].at(0,1) = .1;
  f.weights[0].at(1,1) = 1;

  f.weights[1].at(0,0) = 1;
  f.weights[1].at(1,0) = 0;

  Raw_FeedForward_Network<> raw_net = convert_to_raw(f);
  Raw_FeedForward_Network<> * d_net = network_to_gpu(raw_net);

  arma::Mat<float> inputs(3, 2);
  inputs.at(0,0) = 0;
  inputs.at(0,1) = 0;
  inputs.at(1,0) = 1;
  inputs.at(1,1) = 0;
  inputs.at(2,0) = 0;
  inputs.at(2,1) = 1;

  arma::Mat<float> targets(3,1);
  targets.at(0,0) = 1;
  targets.at(1,0) = 1;
  targets.at(2,0) = 0;

  Raw_Matrix raw_inputs = to_raw(inputs);
  Raw_Matrix * d_inputs = matrix_to_gpu(raw_inputs);

  int num_trials = 3;
  calculate_activation(num_trials, f.layer_sizes, d_net, d_inputs);

  Raw_Matrix raw_targets = to_raw(targets);
  Raw_Matrix * d_targets = matrix_to_gpu(raw_targets);


  backprop(num_trials, f.layer_sizes, d_net, d_targets);
  network_to_cpu_free(d_net, raw_net);

  //Weights calculated from CPU implementation
  ASSERT_NEAR(1.8007, raw_net.weights[1].at(0,0), .1);
  ASSERT_NEAR(-0.0011, raw_net.weights[1].at(1,0), .1);

  ASSERT_NEAR(-1.7962, raw_net.weights[0].at(0,0), .1);
  ASSERT_NEAR(-0.1865, raw_net.weights[0].at(1,0), .1);
  ASSERT_NEAR(0.1800, raw_net.weights[0].at(0,1), .1);
  ASSERT_NEAR(1.800, raw_net.weights[0].at(1,1), .1);
}


TEST(net_gpu, advanced_back_prop) {
  int input_size = 2;
  int hidden_size = 3;
  int output_size = 2;
  FeedForward_Network<> gpu({input_size, hidden_size, output_size});
  FeedForward_Network<> cpu({input_size, hidden_size, output_size});
  gpu.resize_activation(4);
  cpu.resize_activation(4);

  for (int j=0; j < 3; j++) {
    for (int i=0; i < 2; i++) {
      //gpu.weights[0].at(i,j) = .1 * i - .1 * j;
      //cpu.weights[0].at(i,j) = .1 * i - .1 * j;

      gpu.weights[0].at(i,j) = 1 - 1*i - 1*j;
      cpu.weights[0].at(i,j) = 1 - 1*i - 1*j;
      gpu.last_weights[0].at(i,j) = -10;
      cpu.last_weights[0].at(i,j) = -10;

      gpu.weights[1].at(j, i) = -1 - 1*i*j + 1*j;
      cpu.weights[1].at(j, i) = -1 - 1*i*j + 1*j;
      gpu.last_weights[1].at(j, i) = i*(1-j)*1;
      cpu.last_weights[1].at(j, i) = i*(1-j)*1;
    }
  }
  Raw_FeedForward_Network<> raw_net = convert_to_raw(gpu);
  Raw_FeedForward_Network<> * d_net = network_to_gpu(raw_net);

  int num_trials = 4;
  arma::Mat<float> inputs(num_trials, 2);
  inputs.at(0,0) = 0.2;
  inputs.at(0,1) = 0.1;
  inputs.at(1,0) = 1;
  inputs.at(1,1) = .1;
  inputs.at(2,0) = 0;
  inputs.at(2,1) = 1;
  inputs.at(3,0) = 0;
  inputs.at(3,1) = 1;

  arma::Mat<float> targets(num_trials,2);
  targets.at(0,0) = 1;
  targets.at(0,1) = 0;
  targets.at(1,0) = 1;
  targets.at(1,1) = .3;
  targets.at(2,0) = 0;
  targets.at(2,1) = 1;
  targets.at(3,0) = 0;
  targets.at(3,1) = 1;

  Raw_Matrix raw_inputs = to_raw(inputs);
  Raw_Matrix * d_inputs = matrix_to_gpu(raw_inputs);

  calculate_activation(num_trials, gpu.layer_sizes, d_net, d_inputs);

  Raw_Matrix raw_targets = to_raw(targets);
  Raw_Matrix * d_targets = matrix_to_gpu(raw_targets);


  backprop(num_trials, gpu.layer_sizes, d_net, d_targets);
  network_to_cpu_free(d_net, raw_net);
  update_from_raw(gpu, raw_net);

  calculate_activation(cpu, inputs);
  backprop(cpu, targets);

  auto check_equal = [&](arma::Mat<float> A, arma::Mat<float> B) {
    for (int x = 0; x < A.n_rows; x++) {
      for (int y = 0; y < A.n_cols; y++) {
        ASSERT_NEAR(A.at(x,y), B.at(x,y), .005f);
      }
    }
  };

  check_equal(cpu.activations[0], from_raw(raw_net.activations[0]));
  check_equal(cpu.activations[2], from_raw(raw_net.activations[2]));
  check_equal(cpu.activations[1], from_raw(raw_net.activations[1]));
  check_equal(cpu.weights[1], from_raw(raw_net.weights[1]));
  check_equal(cpu.weights[0], from_raw(raw_net.weights[0]));
  check_equal(cpu.last_weights[0], from_raw(raw_net.last_weights[0]));
  check_equal(cpu.last_weights[1], from_raw(raw_net.last_weights[1]));
}


std::array<float, 2> gpu_xor_func(float x, float y) {
  std::array<float, 2> res =  {{static_cast<float>((static_cast<bool>(x)^static_cast<bool>(y))),
                               static_cast<float>(!(static_cast<bool>(x)^static_cast<bool>(y)))}};
  return res;
};


template <typename model_t>
void gpu_check_xor(model_t f) {
  arma::Mat<float> result;

  result = gpu_predict(f, {{0,0}});
  EXPECT_GT(result[1], .9);
  EXPECT_LT(result[0], .1);

  result = gpu_predict(f, {{1,1}});
  EXPECT_GT(result[1], .9);
  EXPECT_LT(result[0], .1);

  result = gpu_predict(f, {{0,1}});
  EXPECT_LT(result[1], .1);
  EXPECT_GT(result[0], .9);

  result = gpu_predict(f, {{1,0}});
  EXPECT_LT(result[1], .1);
  EXPECT_GT(result[0], .9);
}

TEST(net_gpu, reasonable_results_batch_train_xor) {
  FeedForward_Network<> gpu({2, 10, 2});
  randomize(gpu);

  FeedForward_Network<> cpu({2, 10, 2});
  cpu.weights[0] = gpu.weights[0];
  cpu.weights[1] = gpu.weights[1];

  cpu.last_weights[0] = gpu.last_weights[0];
  cpu.last_weights[1] = gpu.last_weights[1];
  const int num_rows = 10000;
  arma::Mat<float> features(num_rows, 2);
  arma::Mat<float> target(num_rows, 2);

  for (int z=0; z < num_rows/4; z++) {
    for (int i=0; i<2; i++) {
      for (int j=0; j<2; j++) {
        int on_index = z*4+i*2+j;
        features(on_index, 0) = i;
        features(on_index, 1) = j;
        target(on_index, 0) = gpu_xor_func(i,j)[0];
        target(on_index, 1) = gpu_xor_func(i,j)[1];
      }
    }
  }

  float learning_rate = 0.8f;
  int batch_size = 1;
  gpu_train_batch(gpu, features, target, batch_size, learning_rate);
  train_batch(cpu, features, target, batch_size, learning_rate);
  gpu_check_xor(gpu);
  gpu_check_xor(cpu);
}

TEST(net_gpu, reasonable_results_batch_train_xor_multipass) {
  FeedForward_Network<> gpu({2, 10, 2});
  randomize(gpu);

  FeedForward_Network<> cpu({2, 10, 2});
  cpu.weights[0] = gpu.weights[0];
  cpu.weights[1] = gpu.weights[1];

  cpu.last_weights[0] = gpu.last_weights[0];
  cpu.last_weights[1] = gpu.last_weights[1];

  const int num_rows = 100;
  arma::Mat<float> features(num_rows, 2);
  arma::Mat<float> target(num_rows, 2);

  for (int z=0; z < num_rows/4; z++) {
    for (int i=0; i<2; i++) {
      for (int j=0; j<2; j++) {
        int on_index = z*4+i*2+j;
        features(on_index, 0) = i;
        features(on_index, 1) = j;
        target(on_index, 0) = gpu_xor_func(i,j)[0];
        target(on_index, 1) = gpu_xor_func(i,j)[1];
      }
    }
  }

  for (int i=0; i < 100; i++) {
    float learning_rate = 0.8f;
    int batch_size = 1;
    gpu_train_batch(gpu, features, target, batch_size, learning_rate);
    train_batch(cpu, features, target, batch_size, learning_rate);
  }
  gpu_check_xor(gpu);
  gpu_check_xor(cpu);
}

std::array<float, 2> gpu_complex_func(float x, float y) {
  x /= 13;
  y /= 13;
  float r1 = x*x + y*y;
  float r2 = x*y + x*y;
  std::array<float, 2> res =  {{
    r1,
    r2
  }};
  return res;
};

TEST(net_gpu, cpu_and_gpu_do_not_diverge_for_circle_function) {
  int input_size =  2;
  int output_size = 2;
  FeedForward_Network<Linear, Squared_Error> gpu({input_size, 10, output_size});
  FeedForward_Network<Linear, Squared_Error> cpu({input_size, 10, output_size});
  randomize(gpu);

  cpu.weights[0] = gpu.weights[0];
  cpu.weights[1] = gpu.weights[1];

  cpu.last_weights[0] = gpu.last_weights[0];
  cpu.last_weights[1] = gpu.last_weights[1];

  const int num_rows = 100;
  arma::Mat<float> features(num_rows, input_size + output_size);

  for (int z=0; z < num_rows; z++) {
    int i = z % 10;
    int j = z / 10;

    features(z, 0) = j/10.f;
    features(z, 1) = i/10.f;

    features(z, input_size+0) = gpu_complex_func(i,j)[0];
    features(z, input_size+1) = gpu_complex_func(i,j)[1];
  }

  float learning_rate = 0.4f;
  int batch_size = 20;

  for (int i=0; i < 100; i++) {
    gpu_train_batch(gpu, features.cols(0, input_size-1), features.cols(input_size, input_size+output_size-1), batch_size, learning_rate);
    train_batch(cpu, features.cols(0, input_size-1), features.cols(input_size, input_size+output_size-1), batch_size, learning_rate);
  }
  arma::Mat<float> gpu_predict_data = gpu_predict(gpu, features.cols(0, input_size-1));
  arma::Mat<float> cpu_predict_data = predict(cpu, features.cols(0, input_size-1));
  float squared_diff_gpu = squared_diff(gpu_predict_data, features.cols(input_size,input_size+output_size-1));
  float squared_diff_cpu = squared_diff(cpu_predict_data, features.cols(input_size,input_size+output_size-1));

  ASSERT_NEAR(squared_diff_gpu, squared_diff_cpu, 1.0f);
}


TEST(net_gpu, cpu_and_gpu_train_on_circle_many_layey) {
  int input_size =  2;
  int output_size = 2;
  FeedForward_Network<Logistic, Squared_Error> gpu({input_size, 10, 10, 10, output_size});
  FeedForward_Network<Logistic, Squared_Error> cpu({input_size, 10, 10, 10, output_size});
  randomize(gpu);

  cpu.weights[0] = gpu.weights[0];
  cpu.weights[1] = gpu.weights[1];

  cpu.last_weights[0] = gpu.last_weights[0];
  cpu.last_weights[1] = gpu.last_weights[1];

  const int num_rows = 100;
  arma::Mat<float> features(num_rows, input_size + output_size);

  for (int z=0; z < num_rows; z++) {
    int i = z % 10;
    int j = z / 10;

    features(z, 0) = j/10.f;
    features(z, 1) = i/10.f;

    features(z, input_size+0) = gpu_complex_func(i,j)[0];
    features(z, input_size+1) = gpu_complex_func(i,j)[1];
  }

  float learning_rate = 0.4f;
  int batch_size = 20;

  for (int i=0; i < 400; i++) {
    gpu_train_batch(gpu, features.cols(0, input_size-1), features.cols(input_size, input_size+output_size-1), batch_size, learning_rate);
    train_batch(cpu, features.cols(0, input_size-1), features.cols(input_size, input_size+output_size-1), batch_size, learning_rate);
  }
  arma::Mat<float> gpu_predict_data = gpu_predict(gpu, features.cols(0, input_size-1));
  arma::Mat<float> cpu_predict_data = predict(cpu, features.cols(0, input_size-1));
  float squared_diff_gpu = squared_diff(gpu_predict_data, features.cols(input_size,input_size+output_size-1));
  float squared_diff_cpu = squared_diff(cpu_predict_data, features.cols(input_size,input_size+output_size-1));

  //TODO
  //FIXME These values appear not to converge. GPu preforms much better than cpu. Maybe different exp implenentations?

  ASSERT_LT(squared_diff_gpu, 3);
  ASSERT_LT(squared_diff_cpu, 3);
}
