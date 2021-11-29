#pragma once


namespace RG
{

using CompileFunc = fu2::unique_function<void(vk::CommandBuffer&, PhysicalResourceProvider&)>;

}
