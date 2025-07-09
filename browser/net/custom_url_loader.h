#ifndef CHROME_BROWSER_NET_CUSTOM_URL_LOADER_H_
#define CHROME_BROWSER_NET_CUSTOM_URL_LOADER_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader.mojom.h"

class CustomURLLoader : public network::mojom::URLLoader {
 public:
  CustomURLLoader(mojo::PendingReceiver<network::mojom::URLLoader> receiver,
                  mojo::PendingRemote<network::mojom::URLLoaderClient> client,
                  const network::ResourceRequest& request);

  ~CustomURLLoader() override;

  // network::mojom::URLLoader overrides:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const absl::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

 private:
  void OnReceiveResponse(network::mojom::URLResponseHeadPtr response_head);
  void OnComplete(const network::URLLoaderCompletionStatus& status);

  mojo::Receiver<network::mojom::URLLoader> receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  network::ResourceRequest request_;
};

#endif  // CHROME_BROWSER_NET_CUSTOM_URL_LOADER_H_
