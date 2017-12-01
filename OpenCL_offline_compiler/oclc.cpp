// oclc.cpp
// g++ -gnu++11 -O3 oclc.cpp -lOpenCL -o oclc
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <memory>
#include <vector>

#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif


static constexpr cl_uint kNDefaultPlatformEntry = 16;
static constexpr cl_uint kNDefaultDeviceEntry = 16;


/*!
 * @brief プラットフォームIDを取得
 * @param [in] nPlatformEntry  取得するプラットフォームID数の上限
 * @return  プラットフォームIDを格納した std::vector
 */
static inline std::vector<cl_platform_id>
getPlatformIds(cl_uint nPlatformEntry = kNDefaultPlatformEntry)
{
  std::vector<cl_platform_id> platformIds(nPlatformEntry);
  cl_uint nPlatform;
  if (clGetPlatformIDs(nPlatformEntry, platformIds.data(), &nPlatform) != CL_SUCCESS) {
    std::cerr << "clGetPlatformIDs() failed" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  platformIds.resize(nPlatform);
  return platformIds;
}


/*!
 * @brief デバイスIDを取得
 * @param [in] platformId    デバイスIDの取得元のプラットフォームのID
 * @param [in] nDeviceEntry  取得するデバイスID数の上限
 * @param [in] deviceType    取得対象とするデバイス
 * @return デバイスIDを格納した std::vector
 */
static inline std::vector<cl_device_id>
getDeviceIds(const cl_platform_id& platformId, cl_uint nDeviceEntry = kNDefaultDeviceEntry, cl_int deviceType = CL_DEVICE_TYPE_DEFAULT)
{
  std::vector<cl_device_id> deviceIds(nDeviceEntry);
  cl_uint nDevice;
  if (clGetDeviceIDs(platformId, deviceType, nDeviceEntry, deviceIds.data(), &nDevice) != CL_SUCCESS) {
    std::cerr << "clGetDeviceIDs() failed" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  deviceIds.resize(nDevice);
  return deviceIds;
}


/*!
 * @brief 指定されたファイル名のファイルを読み込み，std::stringに格納して返却する
 * @param [in] filename  読み込むファイル名
 * @return  ファイルの内容を格納したstd::string
 */
static inline std::string
readSource(const std::string& filename) noexcept
{
  std::ifstream ifs(filename.c_str());
  if (!ifs.is_open()) {
    std::cerr << "Failed to open " << filename << std::endl;
    std::exit(EXIT_FAILURE);
  }
  return std::string((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
}


/*!
 * @brief 指定された複数のファイル名のファイルを読み込み，std::stringに格納して返却する
 * @param [in] filenames  読み込み対象のファイル名を格納したstd::vector
 * @return  ファイルの内容を格納したstd:stringを格納したstd::vector
 */
static inline std::vector<std::string>
readSource(const std::vector<std::string>& filenames)
{
  std::vector<std::string> srcs(filenames.size());
  for (decltype(srcs)::size_type i = 0; i < srcs.size(); i++) {
    srcs[i] = readSource(filenames[i]);
  }
  return srcs;
}


/*!
 * @brief 指定されたファイル名から拡張子を除いた文字列を返却する
 * @param [in] filename  拡張子を取り除きたいファイル名
 * @return  拡張子を除いたファイル名
 */
static inline std::string
removeSuffix(const std::string& filename) noexcept
{
  return filename.substr(0, filename.find_last_of("."));
}


/*!
 * @brief このプログラムのエントリポイント
 * @param [in] argc  コマンドライン引数の数
 * @param [in] argv  コマンドライン引数
 * @return 終了ステータス
 */
int
main(int argc, char* argv[])
{
  // OpenCLのコンパイラに渡すオプション文字列
  // 今回はとりあえず空
  static const char kOptStr[] = "";

  std::vector<std::string> args(argc - 1);
  if (args.size() < 1) {
    std::cerr << "Please specify only one or more source file" << std::endl;
    return EXIT_FAILURE;
  }
  for (decltype(args)::size_type i = 0; i < args.size(); i++) {
    args[i] = std::string(argv[i + 1]);
  }

  // プラットフォームを取得
  std::vector<cl_platform_id> platformIds = getPlatformIds(1);

  // デバイスを取得
  std::vector<cl_device_id> deviceIds = getDeviceIds(platformIds[0], 1, CL_DEVICE_TYPE_DEFAULT);

  // コンテキスト生成
  cl_int errCode;
  std::unique_ptr<std::remove_pointer<cl_context>::type, decltype(&clReleaseContext)> context(
      clCreateContext(nullptr, 1, &deviceIds[0], nullptr, nullptr, &errCode), clReleaseContext);
  if (errCode != CL_SUCCESS) {
    std::cerr << "clCreateContext() failed" << std::endl;
    return EXIT_FAILURE;
  }

  // ソースコード読み込み
  std::vector<std::string> kernelSources = readSource(args);
  std::pair<std::vector<const char*>, std::vector<std::string::size_type> > kernelSourcePairs;
  kernelSourcePairs.first.reserve(kernelSources.size());
  kernelSourcePairs.second.reserve(kernelSources.size());
  for (const auto& kernelSource : kernelSources) {
    kernelSourcePairs.first.emplace_back(kernelSource.c_str());
    kernelSourcePairs.second.emplace_back(kernelSource.length());
  }

  // プログラム生成
  // 複数ソースファイルに対応
  std::unique_ptr<std::remove_pointer<cl_program>::type, decltype(&clReleaseProgram)> program(
      clCreateProgramWithSource(
        context.get(),
        static_cast<cl_uint>(kernelSourcePairs.first.size()),
        kernelSourcePairs.first.data(),
        kernelSourcePairs.second.data(),
        &errCode),
      clReleaseProgram);
  if (errCode != CL_SUCCESS) {
    std::cerr << "clCreateProgramWithSource() failed" << std::endl;
    return EXIT_FAILURE;
  }

  // カーネルソースコードのコンパイル
  switch (clBuildProgram(program.get(), 1, &deviceIds[0], kOptStr, nullptr, nullptr)) {
    case CL_SUCCESS:
      break;
    case CL_BUILD_PROGRAM_FAILURE:
      {
        // コンパイルエラーを表示
        std::array<char, 2048> buildLog;
        std::size_t logSize;
        clGetProgramBuildInfo(program.get(), deviceIds[0], CL_PROGRAM_BUILD_LOG, buildLog.size(), buildLog.data(), &logSize);
        std::cerr << "Compile error:\n" << buildLog.data() << std::endl;
      }
      break;
    case CL_INVALID_BUILD_OPTIONS:
      std::cerr << "Invalid option is specified" << std::endl;
      return EXIT_FAILURE;
    default:
      std::cerr << "clBuildProgram() failed" << std::endl;
      return EXIT_FAILURE;
  }

  // デバイス数を取得 (このプログラムでは1が返却されるはず)
  cl_uint nDevice;
  if (clGetProgramInfo(program.get(), CL_PROGRAM_NUM_DEVICES, sizeof(nDevice), &nDevice, nullptr) !=  CL_SUCCESS) {
    std::cerr << "clGetProgramInfo() failed" << std::endl;
  }

  // 各デバイス向けのコンパイル後のバイナリのサイズを取得
  std::unique_ptr<std::size_t[]> binSizes(new std::size_t[nDevice]);
  if (clGetProgramInfo(program.get(), CL_PROGRAM_BINARY_SIZES, sizeof(std::size_t) * nDevice, binSizes.get(), nullptr) != CL_SUCCESS) {
    std::cerr << "clGetProgramInfo() failed" << std::endl;
  }

  // コンパイル後のバイナリをコピー
  std::vector<std::unique_ptr<char> > bins(nDevice);
  for (std::size_t i = 0; i < nDevice; i++) {
    bins[i] = std::unique_ptr<char>(binSizes[i] == 0 ? nullptr : new char[binSizes[i]]);
  }
  if (clGetProgramInfo(program.get(), CL_PROGRAM_BINARIES, sizeof(char*) * nDevice, bins.data(), nullptr) != CL_SUCCESS) {
    std::cerr << "clGetProgramInfo() failed" << std::endl;
    return EXIT_FAILURE;
  }

  // コピーしたバイナリを全てファイルに出力
  std::string basename = removeSuffix(args[0]);
  for (std::size_t i = 0; i < nDevice; i++) {
    if (bins[i] == nullptr) {
      continue;
    }
    std::string filename = basename + ".bin";
    if (nDevice > 1) {
      filename += "." + std::to_string(i);
    }
    std::ofstream ofs(filename, std::ios::binary);
    if (ofs.is_open()) {
      ofs.write(bins[i].get(), binSizes[i]);
    } else {
      std::cerr << "Failed to open: " << filename << std::endl;
    }
  }

  return EXIT_SUCCESS;
}
