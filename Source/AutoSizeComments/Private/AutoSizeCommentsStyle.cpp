#include "AutoSizeCommentsStyle.h"

#include "AutoSizeCommentsMacros.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

#if ASC_UE_VERSION_OR_LATER(5, 0)
#include "Styling/SlateStyleMacros.h"
#include "Styling/StyleColors.h"
#endif

TSharedPtr<FSlateStyleSet> FASCStyle::Style = nullptr;

#define RootToContentDir Style->RootToContentDir

const FVector2D Icon8x8(8.0f, 8.0f);
const FVector2D Icon16x16(16.0f, 16.0f);
const FVector2D Icon20x20(20.0f, 20.0f);
const FVector2D Icon40x40(40.0f, 40.0f);

void FASCStyle::Initialize()
{
	// Only register once
	if (Style.IsValid())
	{
		return;
	}

	Style = MakeShareable(new FSlateStyleSet("AutoSizeCommentsStyle"));

	Style->SetContentRoot(IPluginManager::Get().FindPlugin("AutoSizeComments")->GetBaseDir() / TEXT("Resources"));

	Style->Set("ASC.AnchorBox", new BOX_BRUSH("AnchorBox", FMargin(18.0f/64.0f), FLinearColor(1.0f, 1.0f, 1.0f, 1.0f)));

#if ASC_UE_VERSION_OR_LATER(5, 0)
	{
		const FTextBlockStyle GraphCommentBlockTitle = FTextBlockStyle()
			.SetFont(DEFAULT_FONT("Bold", 18))
			.SetColorAndOpacity(FLinearColor(218.0f / 255.0f, 218.0f / 255.0f, 218.0f / 255.0f))
			.SetShadowOffset( FVector2D(1.5f, 1.5f) )
			.SetShadowColorAndOpacity( FLinearColor(0.f,0.f,0.f, 0.7f));

		FSlateBrush EmptyBrush;
		EmptyBrush.Margin = FMargin(0);
		EmptyBrush.DrawAs = ESlateBrushDrawType::Type::NoDrawType;

		FScrollBarStyle ScrollBarStyle;
		ScrollBarStyle.Thickness = 0;
		ScrollBarStyle.SetDraggedThumbImage(EmptyBrush);
		ScrollBarStyle.SetHoveredThumbImage(EmptyBrush);
		ScrollBarStyle.SetNormalThumbImage(EmptyBrush);

		const FEditableTextBoxStyle GraphCommentBlockTitleEditableText = FEditableTextBoxStyle()
			.SetPadding(FMargin(0))
			.SetHScrollBarPadding(FMargin(0))
			.SetVScrollBarPadding(FMargin(0))
			.SetBackgroundImageFocused(EmptyBrush)
			.SetBackgroundImageHovered(EmptyBrush)
			.SetBackgroundImageNormal(EmptyBrush)
			.SetBackgroundImageReadOnly(EmptyBrush)
			.SetScrollBarStyle(ScrollBarStyle)
			.SetFont(GraphCommentBlockTitle.Font)
			.SetFocusedForegroundColor(FColor(200, 200, 200, 255));

		const FInlineEditableTextBlockStyle InlineEditableTextBoxStyle = FInlineEditableTextBlockStyle()
			.SetTextStyle(GraphCommentBlockTitle)
			.SetEditableTextBoxStyle(GraphCommentBlockTitleEditableText);

		Style->Set("ASC.CommentTitleTextBoxStyle", InlineEditableTextBoxStyle);

		const FSlateRoundedBoxBrush RoundedBoxBrush(FLinearColor::White, 6.f);
		Style->Set("ASC.PresetButtonStyle", FButtonStyle()
			.SetNormal(RoundedBoxBrush)
			.SetHovered(RoundedBoxBrush)
			.SetPressed(RoundedBoxBrush));
	}
#endif

	FSlateStyleRegistry::RegisterSlateStyle(*Style.Get());
}

#undef RootToContentDir

void FASCStyle::Shutdown()
{
	if (Style.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*Style.Get());
		ensure(Style.IsUnique());
		Style.Reset();
	}
}

const ISlateStyle& FASCStyle::Get()
{
	return *(Style.Get());
}

const FName& FASCStyle::GetStyleSetName()
{
	return Style->GetStyleSetName();
}
