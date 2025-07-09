// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_searchbox_controller.h"

#include "chrome/browser/lens/core/mojom/lens_ghost_loader.mojom.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_session_metrics_logger.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/lens_suggest_inputs_utils.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "net/base/url_util.h"
#include "skia/ext/codec_utils.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

namespace {
// The url query param key for the search query.
inline constexpr char kTextQueryParameterKey[] = "q";
}  // namespace

namespace lens {

LensSearchboxController::LensSearchboxController(
    LensSearchController* lens_search_controller)
    : lens_search_controller_(lens_search_controller) {}
LensSearchboxController::~LensSearchboxController() = default;

void LensSearchboxController::BindOverlayGhostLoader(
    mojo::PendingRemote<lens::mojom::LensGhostLoaderPage> page) {
  overlay_ghost_loader_page_.reset();
  overlay_ghost_loader_page_.Bind(std::move(page));
}

void LensSearchboxController::BindSidePanelGhostLoader(
    mojo::PendingRemote<lens::mojom::LensGhostLoaderPage> page) {
  side_panel_ghost_loader_page_.reset();
  side_panel_ghost_loader_page_.Bind(std::move(page));
}

void LensSearchboxController::OnSessionStart() {
  // Initialize any data needed for the searchbox.
  init_data_ = std::make_unique<LensSearchboxInitializationData>();
}

void LensSearchboxController::SetSidePanelSearchboxHandler(
    std::unique_ptr<LensSearchboxHandler> handler) {
  side_panel_searchbox_handler_ = std::move(handler);
}

void LensSearchboxController::SetContextualSearchboxHandler(
    std::unique_ptr<LensSearchboxHandler> handler) {
  overlay_searchbox_handler_ = std::move(handler);
}

void LensSearchboxController::ResetOverlaySearchboxHandler() {
  overlay_searchbox_handler_.reset();
}

void LensSearchboxController::ResetSidePanelSearchboxHandler() {
  side_panel_searchbox_handler_.reset();
}

void LensSearchboxController::SetSearchboxInputText(const std::string& text) {
  if (side_panel_searchbox_handler_ &&
      side_panel_searchbox_handler_->IsRemoteBound()) {
    init_data_->text_query = text;
    side_panel_searchbox_handler_->SetInputText(text);
  } else {
    // If the side panel was not bound at the time of request, we store the
    // query as pending to send it to the searchbox on bind.
    pending_text_query_ = text;
  }
}

void LensSearchboxController::SetSearchboxThumbnail(
    const std::string& thumbnail_uri) {
  // Init data can be empty if overlay is opened in a normal tab by navigating
  // to the WebUI url in the omnibox.
  if (!init_data_) {
    return;
  }

  // Store the thumbnail.
  init_data_->thumbnail_uri = thumbnail_uri;

  if (side_panel_searchbox_handler_ &&
      side_panel_searchbox_handler_->IsRemoteBound()) {
    side_panel_searchbox_handler_->SetThumbnail(thumbnail_uri);
  }

  if (overlay_searchbox_handler_ &&
      overlay_searchbox_handler_->IsRemoteBound()) {
    overlay_searchbox_handler_->SetThumbnail(thumbnail_uri);
  }
}

void LensSearchboxController::HandleThumbnailCreatedBitmap(
    const SkBitmap& thumbnail) {
  if (thumbnail.drawsNothing()) {
    return;
  }
  std::string image_png = skia::EncodePngAsDataUri(thumbnail.pixmap());
  init_data_->thumbnail_uri = image_png;
  SetSearchboxThumbnail(init_data_->thumbnail_uri);
}

void LensSearchboxController::HandleThumbnailCreated(
    const std::string& thumbnail_bytes) {
  init_data_->thumbnail_uri =
      webui::MakeDataURIForImage(base::as_byte_span(thumbnail_bytes), "jpeg");
  SetSearchboxThumbnail(init_data_->thumbnail_uri);
}

void LensSearchboxController::HandleSuggestInputsResponse(
    lens::proto::LensOverlaySuggestInputs suggest_inputs) {
  if (!init_data_) {
    DCHECK(init_data_)
        << "The initialization data should be set on searchbox startup, which "
           "should have happened before any suggest inputs were received.";
    return;
  }

  // If the handshake was already complete, without the new suggest inputs,
  // exit early so that LensOverlayController::OnHandshakeComplete() isn't
  // called multiple times.
  if (lens_search_controller_->IsHandshakeComplete()) {
    init_data_->suggest_inputs_ = suggest_inputs;
    return;
  }

  // Check if the handshake with the server has been completed with the new
  // inputs. If so, this is the first time the suggest inputs satisfy the
  // handshake criteria, so notify the overlay that the handshake is complete.
  init_data_->suggest_inputs_ = suggest_inputs;
  if (lens_search_controller_->IsHandshakeComplete()) {
    // Notify the overlay that it is now safe to query autocomplete.
    lens_search_controller_->lens_overlay_controller()->OnHandshakeComplete();

    // Send the suggest inputs to any pending callbacks.
    pending_suggest_inputs_callbacks_.Notify(GetLensSuggestInputs());
  }
}

void LensSearchboxController::CloseUI() {
  overlay_searchbox_handler_.reset();
  side_panel_searchbox_handler_.reset();
  overlay_ghost_loader_page_.reset();
  side_panel_ghost_loader_page_.reset();
  init_data_ = std::make_unique<LensSearchboxInitializationData>();
  pending_text_query_ = std::nullopt;
  pending_suggest_inputs_callbacks_.Notify(std::nullopt);
}

bool LensSearchboxController::IsContextualSearchbox() const {
  // TODO(crbug.com/405441183): This logic will break the side panel searchbox
  // if there is no overlay, so it should be moved to a shared location.
  return GetPageClassification() ==
         metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX;
}

bool LensSearchboxController::IsSidePanelSearchbox() const {
  return side_panel_searchbox_handler_ != nullptr;
}

void LensSearchboxController::GetIsContextualSearchbox(
    GetIsContextualSearchboxCallback callback) {
  std::move(callback).Run(IsContextualSearchbox());
}

base::CallbackListSubscription
LensSearchboxController::GetLensSuggestInputsWhenReady(
    ::LensOverlaySuggestInputsCallback callback) {
  // Exit early if the overlay is either off or going to soon be off.
  if (lens_search_controller_->IsClosing() ||
      lens_search_controller_->IsOff()) {
    std::move(callback).Run(std::nullopt);
    return {};
  }

  // If the handshake is complete, return the Lens suggest inputs immediately.
  if (lens_search_controller_->IsHandshakeComplete()) {
    std::move(callback).Run(init_data_->suggest_inputs_);
    return {};
  }
  return pending_suggest_inputs_callbacks_.Add(std::move(callback));
}

const GURL& LensSearchboxController::GetPageURL() const {
  return lens_search_controller_->GetPageURL();
}

SessionID LensSearchboxController::GetTabId() const {
  return sessions::SessionTabHelper::IdForTab(GetTabWebContents());
}

metrics::OmniboxEventProto::PageClassification
LensSearchboxController::GetPageClassification() const {
  // There are two cases where we are assuming to be in a contextual flow:
  // 1) We are in the zero state with the overlay CSB showing.
  // 2) A user has made a contextual query and the live page is now showing.
  // TODO(crbug.com/404941800): Remove dependency on LensOverlayController.
  // Instead, it should check if contextualization is currently active.
  const LensOverlayController::State state =
      lens_search_controller_->lens_overlay_controller()->state();
  if (state == LensOverlayController::State::kLivePageAndResults ||
      state == LensOverlayController::State::kOverlay ||
      lens_search_controller_->lens_search_contextualization_controller()
          ->IsActive()) {
    return metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX;
  }
  return init_data_->thumbnail_uri.empty()
             ? metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX
             : metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX;
}

std::string& LensSearchboxController::GetThumbnail() {
  return init_data_->thumbnail_uri;
}

const lens::proto::LensOverlaySuggestInputs&
LensSearchboxController::GetLensSuggestInputs() const {
  return init_data_
             ? init_data_->suggest_inputs_
             : lens::proto::LensOverlaySuggestInputs().default_instance();
}

void LensSearchboxController::OnTextModified() {
  lens_search_controller_->lens_overlay_controller()->ClearTextSelection();
}

void LensSearchboxController::OnThumbnailRemoved() {
  lens_search_controller_->lens_overlay_controller()->ClearRegionSelection();
}

void LensSearchboxController::OnSuggestionAccepted(
    const GURL& destination_url,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion) {
  base::Time query_start_time = base::Time::Now();
  std::string query_text = "";
  std::map<std::string, std::string> additional_query_parameters;

  net::QueryIterator query_iterator(destination_url);
  while (!query_iterator.IsAtEnd()) {
    std::string_view key = query_iterator.GetKey();
    std::string_view value = query_iterator.GetUnescapedValue();
    if (kTextQueryParameterKey == key) {
      query_text = value;
    } else {
      additional_query_parameters.insert(std::make_pair(
          query_iterator.GetKey(), query_iterator.GetUnescapedValue()));
    }
    query_iterator.Advance();
  }

  // TODO(crbug.com/413138792): Move the logic to issue a searchbox query to
  // this class.
  lens_search_controller_->lens_overlay_controller()->IssueSearchBoxRequest(
      query_start_time, query_text, match_type, is_zero_prefix_suggestion,
      additional_query_parameters);
}

void LensSearchboxController::OnFocusChanged(bool focused) {
  // TOOD(crbug.com/404941800): Implement OnSearchboxFocusChanged logic in this
  // class.
  lens_search_controller_->lens_overlay_controller()->OnSearchboxFocusChanged(
      focused);
}

void LensSearchboxController::OnPageBound() {
  // Send any pending inputs for the searchbox.
  if (pending_text_query_.has_value() && side_panel_searchbox_handler_ &&
      side_panel_searchbox_handler_->IsRemoteBound()) {
    side_panel_searchbox_handler_->SetInputText(*pending_text_query_);
    pending_text_query_.reset();
  }
  // If there is a thumbnail, make sure the searchbox receives it.
  if (init_data_ && !init_data_->thumbnail_uri.empty()) {
    SetSearchboxThumbnail(init_data_->thumbnail_uri);
  }
}

void LensSearchboxController::ShowGhostLoaderErrorState() {
  if (!IsContextualSearchbox()) {
    return;
  }
  if (overlay_ghost_loader_page_) {
    overlay_ghost_loader_page_->ShowErrorState();
  }
  if (side_panel_ghost_loader_page_) {
    side_panel_ghost_loader_page_->ShowErrorState();
  }
}

void LensSearchboxController::OnZeroSuggestShown() {
  if (!IsContextualSearchbox()) {
    return;
  }

  // If this is in the side panel, it is not the initial query.
  lens_search_controller_->lens_session_metrics_logger()->OnZeroSuggestShown(
      /*is_initial_query=*/!IsSidePanelSearchbox());
}

void LensSearchboxController::AddSearchboxStateToSearchQuery(
    lens::SearchQuery& search_query) {
  search_query.selected_region_thumbnail_uri_ = init_data_->thumbnail_uri;
}

content::WebContents* LensSearchboxController::GetTabWebContents() const {
  return lens_search_controller_->GetTabInterface()->GetContents();
}

}  // namespace lens
