#include "chrome/browser/net/custom_url_loader.h"

#include "net/http/http_util.h"

namespace {

const char kStrictCSP[] =
    "default-src 'none'; script-src 'none'; object-src 'none';";

bool ShouldInjectCSP(const GURL& url) {
  return url.spec().find("xss.html") != std::string::npos;
}

}  // namespace

CustomURLLoader::CustomURLLoader(
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const network::ResourceRequest& request)
    : receiver_(this, std::move(receiver)),
      client_(std::move(client)),
      request_(request) {
  receiver_.set_disconnect_handler(base::BindOnce(
      &CustomURLLoader::OnConnectionError, base::Unretained(this)));
}

void CustomURLLoader::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr response_head) {
  if (ShouldInjectCSP(request_.url)) {
    if (!response_head->headers) {
      response_head->headers =
          base::MakeRefCounted<net::HttpResponseHeaders>("");
    }
    response_head->headers->SetHeader("Content-Security-Policy", kStrictCSP);
  }
  client_->OnReceiveResponse(std::move(response_head),
                             mojo::ScopedDataPipeConsumerHandle(),
                             absl::nullopt);
}

void CustomURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const absl::optional<GURL>& new_url) {
  // No-op for this implementation
}

void CustomURLLoader::SetPriority(net::RequestPriority priority,
                                  int32_t intra_priority_value) {
  // No-op for this implementation
}

void CustomURLLoader::PauseReadingBodyFromNet() {
  // No-op for this implementation
}

void CustomURLLoader::ResumeReadingBodyFromNet() {
  // No-op for this implementation
}

void CustomURLLoader::OnConnectionError() {
  delete this;
}
