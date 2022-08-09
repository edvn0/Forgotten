#pragma once

namespace ForgottenEngine::Cast {

template <typename Out, typename In> static constexpr inline Out& as(In* in) { return *static_cast<Out*>(in); }
template <typename Out, typename In> static constexpr inline Out& as(In in) { return *static_cast<Out*>(in); }

} // namespace ForgottenEngine
