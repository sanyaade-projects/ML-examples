﻿//
// Copyright © 2017 Arm Ltd. All rights reserved.
// See LICENSE file in the project root for full license information.
//

#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <memory>
#include <array>
#include <algorithm>
#include "armnn/armnn.hpp"
#include "armnn/Exceptions.hpp"
#include "armnn/Tensor.hpp"
#include "armnn/INetwork.hpp"
#include "armnnTfParser/ITfParser.hpp"

#include "mnist_loader.hpp"


// Helper function to make input tensors
armnn::InputTensors MakeInputTensors(const std::pair<armnn::LayerBindingId,
    armnn::TensorInfo>& input,
    const void* inputTensorData)
{
    return { { input.first, armnn::ConstTensor(input.second, inputTensorData) } };
}

// Helper function to make output tensors
armnn::OutputTensors MakeOutputTensors(const std::pair<armnn::LayerBindingId,
    armnn::TensorInfo>& output,
    void* outputTensorData)
{
    return { { output.first, armnn::Tensor(output.second, outputTensorData) } };
}

int main(int argc, char** argv)
{
    // Load a test image and its correct label
    std::string dataDir = "data/";
    int testImageIndex = 0;
    std::unique_ptr<MnistImage> input = loadMnistImage(dataDir, testImageIndex);
    if (input == nullptr)
        return 1;

    // Import the TensorFlow model. Note: use CreateNetworkFromBinaryFile for .pb files.
    armnnTfParser::ITfParserPtr parser = armnnTfParser::ITfParser::Create();
    armnn::TensorInfo inputTensorInfo({ 1, 784, 1, 1 }, armnn::DataType::Float32);
    armnn::INetworkPtr network = parser->CreateNetworkFromTextFile("model/simple_mnist_tf.prototxt",
                                                                   { {"Placeholder", inputTensorInfo} },
                                                                   { "Softmax" });

    // Find the binding points for the input and output nodes
    armnnTfParser::BindingPointInfo inputBindingInfo = parser->GetNetworkInputBindingInfo("Placeholder");
    armnnTfParser::BindingPointInfo outputBindingInfo = parser->GetNetworkOutputBindingInfo("Softmax");

    // Create a context and optimize the network for a specific compute device, e.g. CpuAcc, GpuAcc
    armnn::IGraphContextPtr context = armnn::IGraphContext::Create(armnn::Compute::CpuAcc);
    armnn::IOptimizedNetworkPtr optNet = armnn::Optimize(*network, context->GetDeviceSpec());

    // Load the optimized network onto the device
    armnn::NetworkId networkIdentifier;
    context->LoadNetwork(networkIdentifier, std::move(optNet));

    // Run a single inference on the test image
    std::array<float, 10> output;
    armnn::Status ret = context->EnqueueWorkload(networkIdentifier,
                                                 MakeInputTensors(inputBindingInfo, &input->image[0]),
                                                 MakeOutputTensors(outputBindingInfo, &output[0]));

    // Convert 1-hot output to an integer label and print
    int label = std::distance(output.begin(), std::max_element(output.begin(), output.end()));
    std::cout << "Predicted: " << label << std::endl;
    std::cout << "   Actual: " << input->label << std::endl;
    return 0;
}