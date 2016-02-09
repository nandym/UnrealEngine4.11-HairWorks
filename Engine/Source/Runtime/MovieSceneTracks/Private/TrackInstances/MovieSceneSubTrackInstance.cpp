// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksPrivatePCH.h"
#include "MovieSceneSubTrack.h"
#include "MovieSceneSubTrackInstance.h"
#include "MovieSceneSequenceInstance.h"
#include "MovieSceneSubSection.h"
#include "IMovieScenePlayer.h"


/* FMovieSceneSubTrackInstance structors
 *****************************************************************************/

FMovieSceneSubTrackInstance::FMovieSceneSubTrackInstance(UMovieSceneSubTrack& InTrack)
	: SubTrack(&InTrack)
{ }


/* IMovieSceneTrackInstance interface
 *****************************************************************************/

void FMovieSceneSubTrackInstance::ClearInstance(IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (const auto& Pair : SequenceInstancesBySection)
	{
		Player.RemoveMovieSceneInstance(*Pair.Key.Get(), Pair.Value.ToSharedRef());
	}
}


void FMovieSceneSubTrackInstance::RefreshInstance(const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	TSet<UMovieSceneSection*> FoundSections;
	const TArray<UMovieSceneSection*>& AllSections = SubTrack->GetAllSections();

	// create instances for sections that need one
	for (const auto Section : AllSections)
	{
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);

		// If the section doesn't have a valid movie scene or no longer has one 
		// (e.g user deleted it) then skip adding an instance for it
		if (SubSection->GetSequence() == nullptr)
		{
			continue;
		}

		FoundSections.Add(Section);

		// create an instance for the section
		TSharedPtr<FMovieSceneSequenceInstance> Instance = SequenceInstancesBySection.FindRef(SubSection);

		if (!Instance.IsValid())
		{
			Instance = MakeShareable(new FMovieSceneSequenceInstance(*SubSection->GetSequence()));
			SequenceInstancesBySection.Add(SubSection, Instance.ToSharedRef());
		}

		Player.AddOrUpdateMovieSceneInstance(*SubSection, Instance.ToSharedRef());
		Instance->RefreshInstance(Player);
	}

	// remove sections that no longer exist
	TMap<TWeakObjectPtr<UMovieSceneSubSection>, TSharedPtr<FMovieSceneSequenceInstance>>::TIterator It = SequenceInstancesBySection.CreateIterator();

	for(; It; ++It)
	{
		if (!FoundSections.Contains(It.Key().Get()))
		{
			Player.RemoveMovieSceneInstance(*It.Key().Get(), It.Value().ToSharedRef());
			It.RemoveCurrent();
		}
	}
}


void FMovieSceneSubTrackInstance::RestoreState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (const auto Section : SubTrack->GetAllSections())
	{
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);
		TSharedPtr<FMovieSceneSequenceInstance> Instance = SequenceInstancesBySection.FindRef(SubSection);

		if (Instance.IsValid())
		{
			Instance->RestoreState(Player);
		}
	}
}


void FMovieSceneSubTrackInstance::SaveState(const TArray<UObject*>& RuntimeObjects, IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance)
{
	for (const auto Section : SubTrack->GetAllSections())
	{
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);
		TSharedPtr<FMovieSceneSequenceInstance> Instance = SequenceInstancesBySection.FindRef(SubSection);

		if (Instance.IsValid())
		{
			Instance->SaveState(Player);
		}
	}
}


TArray<UMovieSceneSection*> FMovieSceneSubTrackInstance::GetAllTraversedSectionsWithPreroll( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime )
{
	TArray<UMovieSceneSection*> TraversedSections;

	bool bPlayingBackwards = CurrentTime - PreviousTime < 0.0f;
	float MaxTime = bPlayingBackwards ? PreviousTime : CurrentTime;
	float MinTime = bPlayingBackwards ? CurrentTime : PreviousTime;

	TRange<float> TraversedRange(MinTime, TRangeBound<float>::Inclusive(MaxTime));

	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		UMovieSceneSection* Section = Sections[SectionIndex];

		float PrerollTime = 0.0f;
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);
		if (SubSection != nullptr)
		{
			PrerollTime = SubSection->PrerollTime;
		}

		if ((Section->GetStartTime()-PrerollTime == CurrentTime) || TraversedRange.Overlaps(TRange<float>(Section->GetRange().GetLowerBoundValue()-PrerollTime, Section->GetRange().GetUpperBoundValue())))
		{
			TraversedSections.Add(Section);
		}
	}

	return TraversedSections;
}

TArray<UMovieSceneSection*> FMovieSceneSubTrackInstance::GetTraversedSectionsWithPreroll( const TArray<UMovieSceneSection*>& Sections, float CurrentTime, float PreviousTime )
{
	TArray<UMovieSceneSection*> TraversedSections = GetAllTraversedSectionsWithPreroll(Sections, CurrentTime, PreviousTime);

	// Remove any overlaps that are underneath another
	for (int32 RemoveAt = 0; RemoveAt < TraversedSections.Num(); )
	{
		UMovieSceneSection* Section = TraversedSections[RemoveAt];
		
		const bool bShouldRemove = TraversedSections.ContainsByPredicate([=](UMovieSceneSection* OtherSection){
			if (Section->GetRowIndex() == OtherSection->GetRowIndex() &&
				Section->GetRange().Overlaps(OtherSection->GetRange()) &&
				Section->GetOverlapPriority() < OtherSection->GetOverlapPriority())
			{
				return true;
			}
			return false;
		});
		
		if (bShouldRemove)
		{
			TraversedSections.RemoveAt(RemoveAt, 1, false);
		}
		else
		{
			++RemoveAt;
		}
	}

	return TraversedSections;
}


void FMovieSceneSubTrackInstance::Update(EMovieSceneUpdateData& UpdateData, const TArray<UObject*>& RuntimeObjects, class IMovieScenePlayer& Player, FMovieSceneSequenceInstance& SequenceInstance) 
{
	const TArray<UMovieSceneSection*>& AllSections = SubTrack->GetAllSections();
	TArray<UMovieSceneSection*> TraversedSections = GetTraversedSectionsWithPreroll(AllSections, UpdateData.Position, UpdateData.LastPosition);

	const float InitialUpdatePosition = UpdateData.Position;

	for (const auto Section : TraversedSections)
	{
		// skip sections with invalid time scale
		UMovieSceneSubSection* SubSection = CastChecked<UMovieSceneSubSection>(Section);

		if (SubSection->TimeScale == 0.0f)
		{
			continue;
		}

		// skip sections without valid instances
		TSharedPtr<FMovieSceneSequenceInstance> Instance = SequenceInstancesBySection.FindRef(SubSection);

		if (!Instance.IsValid())
		{
			continue;
		}

		// calculate section's local time
		const float InstanceOffset = SubSection->StartOffset + Instance->GetTimeRange().GetLowerBoundValue() - SubSection->PrerollTime;
		const float InstanceLastPosition = InstanceOffset + (UpdateData.LastPosition - (SubSection->GetStartTime()- SubSection->PrerollTime)) / SubSection->TimeScale;
		const float InstancePosition = InstanceOffset + (UpdateData.Position - (SubSection->GetStartTime()- SubSection->PrerollTime)) / SubSection->TimeScale;

		EMovieSceneUpdateData SubUpdateData(InstancePosition, InstanceLastPosition);
		SubUpdateData.UpdatePass = UpdateData.UpdatePass;
		SubUpdateData.bPreroll = InitialUpdatePosition < SubSection->GetStartTime();

		// update section
		Instance->Update(SubUpdateData, Player);
	}
}
