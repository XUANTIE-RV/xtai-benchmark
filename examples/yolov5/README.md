# Yolov5示例

## 模型获取

本教程中使用的模型来自[yolov5模型](https://github.com/ultralytics/yolov5)仓库，可通过其 export.py 脚本导出。

```Shell
git clone https://github.com/ultralytics/yolov5.git
cd yolov5
pip3 install ultralytics
python3 export.py --weights yolov5n.pt --include onnx --imgsz 640 640
```

上述命令执行成功后，会在当前目录生成：`yolov5n.onnx`。

## 环境配置

玄铁CPU平台支持矢量/矩阵加速运算，同时支持丰富的元素位宽，可广泛应用于各类对并行计算有较高要求的领域。在玄铁CPU平台部署模型，需要使用HHB工具链在x86上编译模型为可执行文件，然后将可执行文件拷贝到开发板上执行。

### Docker(推荐)

从[XUANTIE 官网](https://www.xrvm.cn/community/download?id=4212696449735004160) 下载安装包后，使用tar解压镜像文件：

```Shell
tar xf hhb-2.11.4.tar.gz
```

切换到解压出来的目录中，使用 docker 命令导入:

```Shell
docker load < hhb-2.11.4.tar
```

创建容器：

```Shell
docker run --privileged --cap-add=SYS_PTRACE --security-opt seccomp=unconfined -itd --name hhb.test.2.11.4 -v /mnt/:/mnt/ yl-hub.eng.t-head.cn/iot-ai/hhb:2.11.4 /bin/bash

docker exec -it hhb.test.2.11.4 /bin/bash
```

进到容器后执行：`hhb --version`, 打印如下输出：

```Shell
HHB version: 2.11.4, build 20240401
```

## 编译模型

### 提前优化（可选）

在使用HHB编译模型之前，可以使用onnxsim简化模型：

```
onnxsim yolov5n.onnx yolov5n_opt.onnx
```

### HHB编译模型

```Shell
hhb --model-file yolov5n_opt.onnx \
    --input-name 'images' \
    --input-shape '1 3 640 640' \
    --output-name '/model.24/m.0/Conv_output_0;/model.24/m.1/Conv_output_0;/model.24/m.2/Conv_output_0' \
    -o yolov5n_out \
    -S \
    --board c908 \
    --model-save run_only \
    --quantization-scheme float16 \
    --calibrate-dataset zidane.jpg \
    -sd zidane.jpg \
    --postprocess save \
    --with-makefile \
```

上述命令的详细说明请参考HHB手册，以下只对关键参数的变种进行说明：

- `-S`: 指定编译模式为simulate，在这种模式下，除了生成模型的二进制文件外，还会调用qemu模拟执行。如果你只想生成可执行程序，可以改为`-D`；
- `--board`: 指定目标板，在当前aibench中，可以指定为`c908` 和 `c907`;
- `--quantization-scheme`: 量化方式，支持`float32`, `float16`, `int8_asym_w_sym`

可以根据需要评估的目标板，修改对应编译参数。

## 执行

将HHB编译的目录拷贝到目标板上，然后执行：

```Shell
cd yolov5n_out
hhb_runtime --loop 10 ./hhb.bm zidane.jpg.0.bin
``