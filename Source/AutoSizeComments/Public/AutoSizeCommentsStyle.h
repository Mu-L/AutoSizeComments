#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FASCStyle
{
public:
	static void Initialize();
	static void Shutdown();

	static const ISlateStyle& Get();
	static const FName& GetStyleSetName();

	static const FSlateBrush* GetBrush(FName PropertyName, const ANSICHAR* Specifier = NULL)
	{
		return Style->GetBrush(PropertyName, Specifier);
	}

private:
	/** Singleton instances of this style. */
	static TSharedPtr<class FSlateStyleSet> Style;
};
