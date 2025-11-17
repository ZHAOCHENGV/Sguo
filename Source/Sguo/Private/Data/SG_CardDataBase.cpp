// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/SG_CardDataBase.h"

FPrimaryAssetId USG_CardDataBase::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Card"), GetFName());
}
