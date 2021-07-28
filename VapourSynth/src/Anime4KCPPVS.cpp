#include <VapourSynth.h>
#include <VSHelper.h>

#include "Anime4KCPP.hpp"

enum class GPGPU
{
    OpenCL, CUDA
};

typedef struct Anime4KCPPData {
    VSNodeRef* node = nullptr;
    VSVideoInfo vi = VSVideoInfo();
    int passes = 2;
    int pushColorCount = 2;
    double strengthColor = 0.3;
    double strengthGradient = 1.0;
    double zoomFactor = 2.0;
    bool GPU = false;
    bool CNN = false;
    bool HDN = false;
    int HDNLevel = 1;
    int pID = 0, dID = 0;
    int OpenCLQueueNum = 4;
    bool OpenCLParallelIO = false;
    Anime4KCPP::ACCreator acCreator;
    Anime4KCPP::Parameters parameters;
    GPGPU GPGPUModel = GPGPU::OpenCL;
}Anime4KCPPData;

static void VS_CC Anime4KCPPInit(VSMap* in, VSMap* out, void** instanceData, VSNode* node, VSCore* core, const VSAPI* vsapi)
{
    Anime4KCPPData* data = (Anime4KCPPData*)(*instanceData);

    if (data->GPU)
    {
        switch (data->GPGPUModel)
        {
        case GPGPU::OpenCL:
#ifdef ENABLE_OPENCL
            if (data->CNN)
                data->acCreator.pushManager<Anime4KCPP::OpenCL::Manager<Anime4KCPP::OpenCL::ACNet>>(
                    data->pID, data->dID, 
                    Anime4KCPP::CNNType::Default, 
                    data->OpenCLQueueNum,
                    data->OpenCLParallelIO);
            else
                data->acCreator.pushManager<Anime4KCPP::OpenCL::Manager<Anime4KCPP::OpenCL::Anime4K09>>(
                    data->pID, data->dID, 
                    data->OpenCLQueueNum,
                    data->OpenCLParallelIO);
#endif // ENABLE_OPENCL
            break;
        case GPGPU::CUDA:
#ifdef ENABLE_CUDA
            data->acCreator.pushManager<Anime4KCPP::Cuda::Manager>(data->dID);
#endif // ENABLE_CUDA
            break;
        }
        data->acCreator.init();
    }

    data->parameters.passes = data->passes;
    data->parameters.pushColorCount = data->pushColorCount;
    data->parameters.strengthColor = data->strengthColor;
    data->parameters.strengthGradient = data->strengthGradient;
    data->parameters.zoomFactor = data->zoomFactor;
    data->parameters.HDN = data->HDN;
    data->parameters.HDNLevel = data->HDNLevel;

    vsapi->setVideoInfo(&data->vi, 1, node);
}

template<typename T>
static const VSFrameRef* VS_CC Anime4KCPPGetFrame(int n, int activationReason, void** instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    Anime4KCPPData* data = (Anime4KCPPData*)(*instanceData);

    if (activationReason == arInitial)
        vsapi->requestFrameFilter(n, data->node, frameCtx);
    else if (activationReason == arAllFramesReady)
    {
        const VSFrameRef* src = vsapi->getFrameFilter(n, data->node, frameCtx);

        size_t srcH = vsapi->getFrameHeight(src, 0);
        size_t srcW = vsapi->getFrameWidth(src, 0);

        size_t srcSrtide = vsapi->getStride(src, 0);

        VSFrameRef* dst = vsapi->newVideoFrame(data->vi.format, data->vi.width, data->vi.height, src, core);

        size_t dstSrtide = vsapi->getStride(dst, 0);

        T* srcR = const_cast<T*>(reinterpret_cast<const T*>(vsapi->getReadPtr(src, 0)));
        T* srcG = const_cast<T*>(reinterpret_cast<const T*>(vsapi->getReadPtr(src, 1)));
        T* srcB = const_cast<T*>(reinterpret_cast<const T*>(vsapi->getReadPtr(src, 2)));

        unsigned char* dstR = vsapi->getWritePtr(dst, 0);
        unsigned char* dstG = vsapi->getWritePtr(dst, 1);
        unsigned char* dstB = vsapi->getWritePtr(dst, 2);

        Anime4KCPP::AC* ac = nullptr;

        if (data->GPU)
        {
            switch (data->GPGPUModel)
            {
            case GPGPU::OpenCL:
#ifdef ENABLE_OPENCL
                if (data->CNN)
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::OpenCL_ACNet);
                else
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::OpenCL_Anime4K09);
#endif
                break;
            case GPGPU::CUDA:
#ifdef ENABLE_CUDA
                if (data->CNN)
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::Cuda_ACNet);
                else
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::Cuda_Anime4K09);
#endif
                break;
            }
        }
        else
        {
            if (data->CNN)
                ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::CPU_ACNet);
            else
                ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::CPU_Anime4K09);
        }

        ac->loadImage(srcH, srcW, srcSrtide, srcR, srcG, srcB);
        ac->process();
        ac->saveImage(dstR, dstSrtide, dstG, dstSrtide, dstB, dstSrtide);

        Anime4KCPP::ACCreator::release(ac);

        vsapi->freeFrame(src);

        return dst;
    }
    return nullptr;
}

template<typename T>
static const VSFrameRef* VS_CC Anime4KCPPGetFrameYUV(int n, int activationReason, void** instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    Anime4KCPPData* data = (Anime4KCPPData*)(*instanceData);

    if (activationReason == arInitial)
        vsapi->requestFrameFilter(n, data->node, frameCtx);
    else if (activationReason == arAllFramesReady)
    {
        const VSFrameRef* src = vsapi->getFrameFilter(n, data->node, frameCtx);

        size_t srcHY = vsapi->getFrameHeight(src, 0);
        size_t srcWY = vsapi->getFrameWidth(src, 0);
        size_t srcHU = vsapi->getFrameHeight(src, 1);
        size_t srcWU = vsapi->getFrameWidth(src, 1);
        size_t srcHV = vsapi->getFrameHeight(src, 2);
        size_t srcWV = vsapi->getFrameWidth(src, 2);

        size_t srcSrtideY = vsapi->getStride(src, 0);
        size_t srcSrtideU = vsapi->getStride(src, 1);
        size_t srcSrtideV = vsapi->getStride(src, 2);

        VSFrameRef* dst = vsapi->newVideoFrame(data->vi.format, data->vi.width, data->vi.height, src, core);

        size_t dstSrtideY = vsapi->getStride(dst, 0);
        size_t dstSrtideU = vsapi->getStride(dst, 1);
        size_t dstSrtideV = vsapi->getStride(dst, 2);

        T* srcY = const_cast<T*>(reinterpret_cast<const T*>(vsapi->getReadPtr(src, 0)));
        T* srcU = const_cast<T*>(reinterpret_cast<const T*>(vsapi->getReadPtr(src, 1)));
        T* srcV = const_cast<T*>(reinterpret_cast<const T*>(vsapi->getReadPtr(src, 2)));

        unsigned char* dstY = vsapi->getWritePtr(dst, 0);
        unsigned char* dstU = vsapi->getWritePtr(dst, 1);
        unsigned char* dstV = vsapi->getWritePtr(dst, 2);

        Anime4KCPP::AC* ac = nullptr;

        if (data->GPU)
        {
            switch (data->GPGPUModel)
            {
            case GPGPU::OpenCL:
#ifdef ENABLE_OPENCL
                if (data->CNN)
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::OpenCL_ACNet);
                else
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::OpenCL_Anime4K09);
#endif
                break;
            case GPGPU::CUDA:
#ifdef ENABLE_CUDA
                if (data->CNN)
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::Cuda_ACNet);
                else
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::Cuda_Anime4K09);
#endif
                break;
            }
        }
        else
        {
            if (data->CNN)
                ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::CPU_ACNet);
            else
                ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::CPU_Anime4K09);
        }

        ac->loadImage(
            srcHY, srcWY, srcSrtideY, srcY, 
            srcHU, srcWU, srcSrtideU, srcU, 
            srcHV, srcWV, srcSrtideV, srcV);
        ac->process();
        ac->saveImage(dstY, dstSrtideY, dstU, dstSrtideU, dstV, dstSrtideV);

        Anime4KCPP::ACCreator::release(ac);

        vsapi->freeFrame(src);

        return dst;
    }
    return nullptr;
}

template<typename T>
static const VSFrameRef* VS_CC Anime4KCPPGetFrameGrayscale(int n, int activationReason, void** instanceData, void** frameData, VSFrameContext* frameCtx, VSCore* core, const VSAPI* vsapi)
{
    Anime4KCPPData* data = (Anime4KCPPData*)(*instanceData);

    if (activationReason == arInitial)
        vsapi->requestFrameFilter(n, data->node, frameCtx);
    else if (activationReason == arAllFramesReady)
    {
        const VSFrameRef* src = vsapi->getFrameFilter(n, data->node, frameCtx);

        size_t srcH = vsapi->getFrameHeight(src, 0);
        size_t srcW = vsapi->getFrameWidth(src, 0);

        size_t srcSrtide = vsapi->getStride(src, 0);

        VSFrameRef* dst = vsapi->newVideoFrame(data->vi.format, data->vi.width, data->vi.height, src, core);

        size_t dstSrtide = vsapi->getStride(dst, 0);

        T* srcY = const_cast<T*>(reinterpret_cast<const T*>(vsapi->getReadPtr(src, 0)));

        unsigned char* dstY = vsapi->getWritePtr(dst, 0);

        Anime4KCPP::AC* ac = nullptr;

        if (data->GPU)
        {
            switch (data->GPGPUModel)
            {
            case GPGPU::OpenCL:
#ifdef ENABLE_OPENCL
                if (data->CNN)
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::OpenCL_ACNet);
                else
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::OpenCL_Anime4K09);
#endif
                break;
            case GPGPU::CUDA:
#ifdef ENABLE_CUDA
                if (data->CNN)
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::Cuda_ACNet);
                else
                    ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::Cuda_Anime4K09);
#endif
                break;
            }
        }
        else
        {
            if (data->CNN)
                ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::CPU_ACNet);
            else
                ac = Anime4KCPP::ACCreator::create(data->parameters, Anime4KCPP::Processor::Type::CPU_Anime4K09);
        }

        ac->loadImage(srcH, srcW, srcSrtide, srcY, false, false, true);
        ac->process();
        ac->saveImage(dstY, dstSrtide);

        Anime4KCPP::ACCreator::release(ac);

        vsapi->freeFrame(src);

        return dst;
    }
    return nullptr;
}

static void VS_CC Anime4KCPPFree(void* instanceData, VSCore* core, const VSAPI* vsapi)
{
    Anime4KCPPData* data = (Anime4KCPPData*)instanceData;
    vsapi->freeNode(data->node);
    delete data;
}

static void VS_CC Anime4KCPPCreate(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
    Anime4KCPPData tmpData;
    int err = 0;

    tmpData.node = vsapi->propGetNode(in, "src", 0, 0);
    tmpData.vi = *vsapi->getVideoInfo(tmpData.node);

    if (!isConstantFormat(&tmpData.vi) ||
        (tmpData.vi.format->colorFamily != cmYUV && tmpData.vi.format->colorFamily != cmRGB && tmpData.vi.format->colorFamily != cmGray) ||
        (tmpData.vi.format->bitsPerSample != 8 && tmpData.vi.format->bitsPerSample != 16 && tmpData.vi.format->bitsPerSample != 32))
    {
        vsapi->setError(out, 
            "Anime4KCPP: supported data type: RGB, YUV and Grayscale, depth with 8 or 16bit integer or 32bit float)");
        vsapi->freeNode(tmpData.node);
        return;
    }

    tmpData.passes = vsapi->propGetInt(in, "passes", 0, &err);
    if (err)
        tmpData.passes = 2;

    tmpData.pushColorCount = vsapi->propGetInt(in, "pushColorCount", 0, &err);
    if (err)
        tmpData.pushColorCount = 2;

    tmpData.strengthColor = vsapi->propGetFloat(in, "strengthColor", 0, &err);
    if (err)
        tmpData.strengthColor = 0.3;
    else if (tmpData.strengthColor < 0.0 || tmpData.strengthColor > 1.0)
    {
        vsapi->setError(out, "Anime4KCPP: strengthColor must range from 0 to 1");
        vsapi->freeNode(tmpData.node);
        return;
    }

    tmpData.strengthGradient = vsapi->propGetFloat(in, "strengthGradient", 0, &err);
    if (err)
        tmpData.strengthGradient = 1.0;
    else if (tmpData.strengthGradient < 0.0 || tmpData.strengthGradient > 1.0)
    {
        vsapi->setError(out, "Anime4KCPP: strengthGradient must range from 0 to 1");
        vsapi->freeNode(tmpData.node);
        return;
    }

    tmpData.zoomFactor = vsapi->propGetFloat(in, "zoomFactor", 0, &err);
    if (err)
        tmpData.zoomFactor = 2.0;
    else if (tmpData.zoomFactor < 1.0)
    {
        vsapi->setError(out, "Anime4KCPP: zoomFactor must >= 1.0");
        vsapi->freeNode(tmpData.node);
        return;
    }

    tmpData.CNN = vsapi->propGetInt(in, "ACNet", 0, &err);
    if (err)
        tmpData.CNN = false;

    if (tmpData.vi.format->id != pfGray16 &&
        tmpData.vi.format->id != pfGray8 &&
        tmpData.vi.format->id != pfGrayS &&
        tmpData.vi.format->id != pfRGB24 && 
        tmpData.vi.format->id != pfRGB48 &&
        tmpData.vi.format->id != pfRGBS &&
        tmpData.vi.format->id != pfYUV444P8 && 
        tmpData.vi.format->id != pfYUV444P16 && 
        tmpData.vi.format->id != pfYUV444PS &&
        tmpData.CNN == false)
    {
        vsapi->setError(out, "Anime4KCPP: RGB or YUV444 or Grayscale only for Anime4K09");
        vsapi->freeNode(tmpData.node);
        return;
    }

    tmpData.HDN = vsapi->propGetInt(in, "HDN", 0, &err);
    if (err)
        tmpData.HDN = false;

    tmpData.HDNLevel = vsapi->propGetInt(in, "HDNLevel", 0, &err);
    if (err)
        tmpData.HDNLevel = 1;
    else if (tmpData.HDNLevel < 1 || tmpData.HDNLevel > 3)
    {
        vsapi->setError(out, "Anime4KCPP: HDNLevel must range from 1 to 3");
        vsapi->freeNode(tmpData.node);
        return;
    }

    tmpData.GPU = vsapi->propGetInt(in, "GPUMode", 0, &err);
    if (err)
        tmpData.GPU = false;

    std::string GPGPUModel;
    const char* tmpStr = vsapi->propGetData(in, "GPGPUModel", 0, &err);
    if (err)
#ifdef ENABLE_OPENCL
        GPGPUModel = "opencl";
#elif defined(ENABLE_CUDA)
        GPGPUModel = "cuda";
#else
        GPGPUModel = "cpu";
#endif

    else
        GPGPUModel = tmpStr;
    std::transform(GPGPUModel.begin(), GPGPUModel.end(), GPGPUModel.begin(), ::tolower);

    if (GPGPUModel == "opencl")
    {
#ifndef ENABLE_OPENCL
        vsapi->setError(out, "Anime4KCPP: OpenCL is unsupported");
        vsapi->freeNode(tmpData.node);
        return;
#endif // !ENABLE_OPENCL
        tmpData.GPGPUModel = GPGPU::OpenCL;
    }
    else if(GPGPUModel == "cuda")
    {
        
#ifndef ENABLE_CUDA
        vsapi->setError(out, "Anime4KCPP: CUDA is unsupported");
        vsapi->freeNode(tmpData.node);
        return;
#endif // !ENABLE_CUDA
        tmpData.GPGPUModel = GPGPU::CUDA;
    }
    else if (GPGPUModel == "cpu")
    {
        if (tmpData.GPU)
        {
            vsapi->setError(out, "Anime4KCPP: OpenCL or CUDA is unsupported");
            vsapi->freeNode(tmpData.node);
            return;
        }
    }
    else
    {
        vsapi->setError(out, "Anime4KCPP: GPGPUModel must be \"cuda\" or \"opencl\"");
        vsapi->freeNode(tmpData.node);
        return;
    }

    tmpData.pID = vsapi->propGetInt(in, "platformID", 0, &err);
    if (err || !tmpData.GPU)
        tmpData.pID = 0;

    tmpData.dID = vsapi->propGetInt(in, "deviceID", 0, &err);
    if (err || !tmpData.GPU)
        tmpData.dID = 0;

    tmpData.OpenCLQueueNum = vsapi->propGetInt(in, "OpenCLQueueNum", 0, &err);
    if (err)
        tmpData.OpenCLQueueNum = 4;
    else if (tmpData.OpenCLQueueNum < 1)
    {
        vsapi->setError(out, "Anime4KCPP: OpenCLQueueNum must >= 1");
        vsapi->freeNode(tmpData.node);
        return;
    }

    tmpData.OpenCLParallelIO = vsapi->propGetInt(in, "OpenCLParallelIO", 0, &err);
    if (err)
        tmpData.OpenCLParallelIO = false;

    if (tmpData.GPU)
    {
        std::string info;
        try
        {
            switch (tmpData.GPGPUModel)
            {
            case GPGPU::OpenCL:
#ifdef ENABLE_OPENCL
            {
                Anime4KCPP::OpenCL::GPUList list = Anime4KCPP::OpenCL::listGPUs();
                if (tmpData.pID >= list.platforms ||
                    tmpData.dID >= list[tmpData.pID])
                {
                    std::ostringstream err;
                    err << "Platform ID or device ID index out of range" << std::endl
                        << "Run core.anim4kcpp.listGPUs for available platforms and devices" << std::endl
                        << "Your input is: " << std::endl
                        << "    platform ID: " << tmpData.pID << std::endl
                        << "    device ID: " << tmpData.dID << std::endl;
                    vsapi->setError(out, err.str().c_str());
                    vsapi->freeNode(tmpData.node);
                    return;
                }
                Anime4KCPP::OpenCL::GPUInfo ret =
                    Anime4KCPP::OpenCL::checkGPUSupport(tmpData.pID, tmpData.dID);
                if (!ret)
                {
                    std::ostringstream err;
                    err << "The current device is unavailable" << std::endl
                        << "Your input is: " << std::endl
                        << "    platform ID: " << tmpData.pID << std::endl
                        << "    device ID: " << tmpData.dID << std::endl
                        << "Error: " << std::endl
                        << "    " + ret() << std::endl;
                    vsapi->setError(out, err.str().c_str());
                    vsapi->freeNode(tmpData.node);
                    return;
                }
                info = ret();
            }
#endif
            break;
            case GPGPU::CUDA:
#ifdef ENABLE_CUDA
            {
                Anime4KCPP::Cuda::GPUList list = Anime4KCPP::Cuda::listGPUs();
                if (list.devices == 0)
                {
                    vsapi->setError(out, list().c_str());
                    vsapi->freeNode(tmpData.node);
                    return;
                }
                else if (tmpData.dID >= list.devices)
                {
                    std::ostringstream err;
                    err << "Device ID index out of range" << std::endl
                        << "Run core.anim4kcpp.listGPUs for available CUDA devices" << std::endl
                        << "Your input is: " << std::endl
                        << "    device ID: " << tmpData.dID << std::endl;
                    vsapi->setError(out, err.str().c_str());
                    vsapi->freeNode(tmpData.node);
                    return;
                }
                Anime4KCPP::Cuda::GPUInfo ret =
                    Anime4KCPP::Cuda::checkGPUSupport(tmpData.dID);
                if (!ret)
                {
                    std::ostringstream err;
                    err << "The current device is unavailable" << std::endl
                        << "Your input is: " << std::endl
                        << "    device ID: " << tmpData.dID << std::endl
                        << "Error: " << std::endl
                        << "    " + ret() << std::endl;
                    vsapi->setError(out, err.str().c_str());
                    vsapi->freeNode(tmpData.node);
                    return;
                }
                info = ret();
            }
#endif
            break;
            }
        }
        catch (const std::exception& err)
        {
            vsapi->setError(out, err.what());
            vsapi->freeNode(tmpData.node);
            return;
        }
        vsapi->logMessage(mtDebug, ("Current GPU information: \n" + info).c_str());
    }

    if (tmpData.zoomFactor != 1.0)
    {
        tmpData.vi.width = std::round(tmpData.vi.width * tmpData.zoomFactor);
        tmpData.vi.height = std::round(tmpData.vi.height * tmpData.zoomFactor);
    }

    Anime4KCPPData* data = new Anime4KCPPData;
    *data = tmpData;

    if (tmpData.vi.format->colorFamily == cmYUV)
    {
        if (tmpData.vi.format->sampleType == stFloat)
            vsapi->createFilter(in, out, "Anime4KCPP", Anime4KCPPInit, Anime4KCPPGetFrameYUV<float>, Anime4KCPPFree, fmParallel, 0, data, core);
        else
            if (tmpData.vi.format->bitsPerSample == 8)
                vsapi->createFilter(in, out, "Anime4KCPP", Anime4KCPPInit, Anime4KCPPGetFrameYUV<unsigned char>, Anime4KCPPFree, fmParallel, 0, data, core);
            else
                vsapi->createFilter(in, out, "Anime4KCPP", Anime4KCPPInit, Anime4KCPPGetFrameYUV<unsigned short>, Anime4KCPPFree, fmParallel, 0, data, core);
    }
    else if (tmpData.vi.format->colorFamily == cmGray)
    {
        if (tmpData.vi.format->sampleType == stFloat)
            vsapi->createFilter(in, out, "Anime4KCPP", Anime4KCPPInit, Anime4KCPPGetFrameGrayscale<float>, Anime4KCPPFree, fmParallel, 0, data, core);
        else
            if (tmpData.vi.format->bitsPerSample == 8)
                vsapi->createFilter(in, out, "Anime4KCPP", Anime4KCPPInit, Anime4KCPPGetFrameGrayscale<unsigned char>, Anime4KCPPFree, fmParallel, 0, data, core);
            else
                vsapi->createFilter(in, out, "Anime4KCPP", Anime4KCPPInit, Anime4KCPPGetFrameGrayscale<unsigned short>, Anime4KCPPFree, fmParallel, 0, data, core);
    }
    else
    {
        if (tmpData.vi.format->sampleType == stFloat)
            vsapi->createFilter(in, out, "Anime4KCPP", Anime4KCPPInit, Anime4KCPPGetFrame<float>, Anime4KCPPFree, fmParallel, 0, data, core);
        else
            if (tmpData.vi.format->bitsPerSample == 8)
                vsapi->createFilter(in, out, "Anime4KCPP", Anime4KCPPInit, Anime4KCPPGetFrame<unsigned char>, Anime4KCPPFree, fmParallel, 0, data, core);
            else
                vsapi->createFilter(in, out, "Anime4KCPP", Anime4KCPPInit, Anime4KCPPGetFrame<unsigned short>, Anime4KCPPFree, fmParallel, 0, data, core);
    }
}

static void VS_CC Anime4KCPPListGPUs(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
    int err;
    std::string GPGPUModel;
    const char * tmpStr = vsapi->propGetData(in, "GPGPUModel", 0, &err);
    if (err)
        GPGPUModel = "opencl";
    else
        GPGPUModel = tmpStr;
    std::transform(GPGPUModel.begin(), GPGPUModel.end(), GPGPUModel.begin(), ::tolower);

#ifdef ENABLE_OPENCL
    if(GPGPUModel=="opencl")
        vsapi->logMessage(mtDebug, Anime4KCPP::OpenCL::listGPUs()().c_str());
    else
#endif // ENABLE_OPENCL
#ifdef ENABLE_CUDA
        if (GPGPUModel == "cuda")
            vsapi->logMessage(mtDebug, Anime4KCPP::Cuda::listGPUs()().c_str());
        else
#endif // ENABLE_CUDA
        vsapi->logMessage(mtDebug, "unkonwn GPGPUModel module");
}

static void VS_CC Anime4KCPPBenchmark(const VSMap* in, VSMap* out, void* userData, VSCore* core, const VSAPI* vsapi)
{
    int err = 0;
    int pID = vsapi->propGetInt(in, "platformID", 0, &err);
    if (err || !pID)
        pID = 0;

    int dID = vsapi->propGetInt(in, "deviceID", 0, &err);
    if (err || !dID)
        dID = 0;

    double CPUScoreDVD = Anime4KCPP::benchmark<Anime4KCPP::CPU::ACNet, 720, 480>();
    double CPUScoreHD = Anime4KCPP::benchmark<Anime4KCPP::CPU::ACNet, 1280, 720>();
    double CPUScoreFHD = Anime4KCPP::benchmark<Anime4KCPP::CPU::ACNet, 1920, 1080>();

#ifdef ENABLE_OPENCL
    double OpenCLScoreDVD = Anime4KCPP::benchmark<Anime4KCPP::OpenCL::ACNet, 720, 480>(pID, dID);
    double OpenCLScoreHD = Anime4KCPP::benchmark<Anime4KCPP::OpenCL::ACNet, 1280, 720>(pID, dID);
    double OpenCLScoreFHD = Anime4KCPP::benchmark<Anime4KCPP::OpenCL::ACNet, 1920, 1080>(pID, dID);
#endif 

#ifdef ENABLE_CUDA
    double CudaScoreDVD = Anime4KCPP::benchmark<Anime4KCPP::Cuda::ACNet, 720, 480>(dID);
    double CudaScoreHD = Anime4KCPP::benchmark<Anime4KCPP::Cuda::ACNet, 1280, 720>(dID);
    double CudaScoreFHD = Anime4KCPP::benchmark<Anime4KCPP::Cuda::ACNet, 1920, 1080>(dID);
#endif 

    std::ostringstream oss;

    oss << "Benchmark test under 8-bit integer input and serial processing..." << std::endl << std::endl;

    oss
        << "CPU score:" << std::endl
        << " DVD(480P->960P): " << CPUScoreDVD << " FPS" << std::endl
        << " HD(720P->1440P): " << CPUScoreHD << " FPS" << std::endl
        << " FHD(1080P->2160P): " << CPUScoreFHD << " FPS" << std::endl << std::endl;

#ifdef ENABLE_OPENCL
    oss
        << "OpenCL score:" << " (pID = " << pID << ", dID = " << dID << ")" << std::endl
        << " DVD(480P->960P): " << OpenCLScoreDVD << " FPS" << std::endl
        << " HD(720P->1440P): " << OpenCLScoreHD << " FPS" << std::endl
        << " FHD(1080P->2160P): " << OpenCLScoreFHD << " FPS" << std::endl << std::endl;
#endif

#ifdef ENABLE_CUDA
    oss
        << "CUDA score:" << " (dID = " << dID << ")" << std::endl
        << " DVD(480P->960P): " << CudaScoreDVD << " FPS" << std::endl
        << " HD(720P->1440P): " << CudaScoreHD << " FPS" << std::endl
        << " FHD(1080P->2160P): " << CudaScoreFHD << " FPS" << std::endl << std::endl;
#endif 

    vsapi->logMessage(mtDebug, oss.str().c_str());
}

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin* plugin)
{
    configFunc("github.tianzerl.anime4kcpp", "anime4kcpp", "Anime4KCPP for VapourSynth", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Anime4KCPP",
        "src:clip;"
        "passes:int:opt;"
        "pushColorCount:int:opt;"
        "strengthColor:float:opt;"
        "strengthGradient:float:opt;"
        "zoomFactor:float:opt;"
        "ACNet:int:opt;"
        "GPUMode:int:opt;"
        "GPGPUModel:data:opt;"
        "HDN:int:opt;"
        "HDNLevel:int:opt;"
        "platformID:int:opt;"
        "deviceID:int:opt;"
        "OpenCLQueueNum:int:opt;"
        "OpenCLParallelIO:int:opt;",
        Anime4KCPPCreate, nullptr, plugin);

    registerFunc("listGPUs", "GPGPUModel:data:opt", Anime4KCPPListGPUs, nullptr, plugin);

    registerFunc("benchmark",
        "platformID:int:opt;"
        "deviceID:int:opt;",
        Anime4KCPPBenchmark, nullptr, plugin);
}
