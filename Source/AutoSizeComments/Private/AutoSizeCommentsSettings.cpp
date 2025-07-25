// Copyright fpwong. All Rights Reserved.

#include "AutoSizeCommentsSettings.h"

#include "AutoSizeCommentsCacheFile.h"
#include "AutoSizeCommentsGraphHandler.h"
#include "AutoSizeCommentsMacros.h"
#include "AutoSizeCommentsState.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SButton.h"

UAutoSizeCommentsSettings::UAutoSizeCommentsSettings(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	ResizingMode = EASCResizingMode::Reactive;
	ResizeToFitWhenDisabled = false;
	bUseTwoPassResize = true;
	AutoInsertComment = EASCAutoInsertComment::Always;
	bSelectNodeWhenClickingOnPin = true;
	bAutoRenameNewComments = true;
	CommentNodePadding = FVector2D(30, 30);
	MinimumVerticalPadding = 24.0f;
	CommentTextPadding = FMargin(2, 0, 2, 0);
	CommentTextAlignment = ETextJustify::Left;
	DefaultFontSize = 18;
	bUseDefaultFontSize = false;
	DefaultCommentColorMethod = EASCDefaultCommentColorMethod::None;
	HeaderColorMethod = EASCDefaultCommentColorMethod::Default;
	RandomColorOpacity = 1.f;
	bUseRandomColorFromList = false;
	PredefinedRandomColorList.Add(FLinearColor(1, 0, 0));
	PredefinedRandomColorList.Add(FLinearColor(0, 1, 0));
	PredefinedRandomColorList.Add(FLinearColor(0, 0, 1));
	PredefinedRandomColorList.Add(FLinearColor(0, 1, 1));
	PredefinedRandomColorList.Add(FLinearColor(1, 1, 0));
	PredefinedRandomColorList.Add(FLinearColor(0, 1, 1));
	PredefinedRandomColorList.Add(FLinearColor(1, 0, 1));
	MinimumControlOpacity = 0.f;
	DefaultCommentColor = FLinearColor::White;
	HeaderStyle.Color = FLinearColor::Gray;

	// define tagged preset
	{
		FPresetCommentStyle TodoPreset;
		TodoPreset.Color = FColor(0, 255, 255);
		TaggedPresets.Add("@TODO", TodoPreset);

		FPresetCommentStyle FixmePreset;
		FixmePreset.Color = FColor::Red;
		TaggedPresets.Add("@FIXME", FixmePreset);

		FPresetCommentStyle InfoPreset;
		InfoPreset.Color = FColor::White;
		InfoPreset.bSetHeader = true;
		TaggedPresets.Add("@INFO", InfoPreset);
	}
	bAggressivelyUseDefaultColor = false;
	bUseCommentBubbleBounds = true;
	bMoveEmptyCommentBoxes = false;
	EmptyCommentBoxSpeed = 10;
	bHideCommentBubble = false;
	bEnableCommentBubbleDefaults = false;
	bDefaultColorCommentBubble = false;
	bDefaultShowBubbleWhenZoomed = true;
	CacheSaveMethod = EASCCacheSaveMethod::MetaData;
	CacheSaveLocation = EASCCacheSaveLocation::Project;
	bSaveCommentDataOnSavingGraph = true;
	bSaveCommentDataOnExit = false;
	bPrettyPrintCommentCacheJSON = false;
	bApplyColorToExistingNodes = false;
	bResizeExistingNodes = false;
	bDetectNodesContainedForNewComments = true;
	ResizeChord = FInputChord(EKeys::LeftMouseButton, EModifierKey::Shift);
	ResizeCollisionMethod = ECommentCollisionMethod::Contained;
	EnableCommentControlsKey = FInputChord();
	AltCollisionMethod = ECommentCollisionMethod::Intersect;
	ResizeCornerAnchorSize = 40.0f;
	ResizeSidePadding = 20.0f;
	bSnapToGridWhileResizing = false;
	bIgnoreKnotNodes = false;
	bIgnoreKnotNodesWhenPressingAlt = false;
	bIgnoreKnotNodesWhenResizing = false;
	bIgnoreSelectedNodesOnCreation = false;
	bRefreshContainingNodesOnMove = false;
	bDisableTooltip = true;
	bHighlightContainingNodesOnSelection = true;
	bUseMaxDetailNodes = false;
	IgnoredGraphs.Add("ControlRigGraph");
	bSuppressSuggestedSettings = false;
	bSuppressSourceControlNotification = false;
	bHideResizeButton = false;
	bHideHeaderButton = false;
	bHideCommentBoxControls = false;
	bHidePresets = false;
	bHideRandomizeButton = false;
	bHideCornerPoints = false;

	bEnableFixForSortDepthIssue = false;

	bDebugGraph_ASC = false;
	bDisablePackageCleanup = false;
	bDisableASCGraphNode = false;
}

void UAutoSizeCommentsSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UAutoSizeCommentsSettings, bHighlightContainingNodesOnSelection))
	{
		if (!bHighlightContainingNodesOnSelection)
		{
			FAutoSizeCommentGraphHandler::Get().ClearUnrelatedNodes();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

TSharedRef<IDetailCustomization> FASCSettingsDetails::MakeInstance()
{
	return MakeShareable(new FASCSettingsDetails);
}
void FASCSettingsDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	IDetailCategoryBuilder& GeneralCategory = DetailBuilder.EditCategory("CommentCache");
	auto& SizeCache = FAutoSizeCommentsCacheFile::Get();

	const FString CachePath = SizeCache.GetCachePath(true);

	const auto DeleteSizeCache = [&SizeCache]()
	{
		static FText Title = INVTEXT("Clear comment cache");
		static FText Message = INVTEXT("Are you sure you want to delete the comment cache?");

#if ASC_UE_VERSION_OR_LATER(5, 3)
		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Message, Title);
#else
		const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, Message, &Title);
#endif
		if (Result == EAppReturnType::Yes)
		{
			SizeCache.DeleteCache();
		}

#if 0 // Testing to full clear all cache data 
		FASCState::Get().CommentToASCMapping.Empty();
		FAutoSizeCommentGraphHandler::Get().ClearGraphData();
#endif

		return FReply::Handled();
	};

	GeneralCategory.AddCustomRow(INVTEXT("Clear comment cache"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(INVTEXT("Clear comment cache"))
			.Font(ASC_GET_FONT_STYLE(TEXT("PropertyWindow.NormalFont")))
		]
		.ValueContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().Padding(5).AutoWidth()
			[
				SNew(SButton)
				.Text(INVTEXT("Clear comment cache"))
				.ToolTipText(FText::FromString(FString::Printf(TEXT("Delete comment cache file located at: %s"), *CachePath)))
				.OnClicked_Lambda(DeleteSizeCache)
			]
		];
	
}
