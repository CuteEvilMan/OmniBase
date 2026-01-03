# FluxBase

高性能 C++17 命令行工具，用自定义字符集将任意二进制流编码为字符串流，并支持解码还原。

## 构建
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 用法示例
```bash
# 编码
./build/fluxbase encode \
  --input source.dat \
  --output encoded.txt \
  --charset "ABCDEFGH01234567" \
  --pow2 \
  --block 8

# 解码（带头）
./build/fluxbase decode \
  --input encoded.txt \
  --output restored.dat

# 解码（无头，需要补参数）
./build/fluxbase decode \
  --input encoded.txt \
  --output restored.dat \
  --charset "ABCDEFGH01234567" \
  --pow2 \
  --block 8 \
  --no-header
```

## 注意
- charset 自动去重，`--pow2` 会截断到最大 2 的幂。
- `--block` 以字节为单位，默认 8。
- 默认输出写入文件头，`--no-header` 禁用。
