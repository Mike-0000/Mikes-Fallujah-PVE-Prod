modded class SCR_AmbientPatrolSystem : GameSystem
{
	protected SCR_AIGroup spawnedGroup;	// Added by Gramps
	bool factionProcessed = false;
	SCR_CampaignMilitaryBaseComponent hqBase;
	
	override protected void ProcessSpawnpoint(int spawnpointIndex)
	{
		SCR_AmbientPatrolSpawnPointComponent spawnpoint = m_aPatrols[spawnpointIndex];

		if (!spawnpoint || (spawnpoint.GetMembersAlive() == 0 && !spawnpoint.GetIsSpawned()))
			return;

		ChimeraWorld world = GetWorld();
		WorldTimestamp currentTime = world.GetServerTimestamp();
		if (spawnpoint.GetRespawnTimestamp().Greater(currentTime))
			return;

/* Gramps added - If vehicle group, TP to parent vehicle from vehicle spawnpoint and adjust group size to fill vehicle >> */				
		if(spawnpoint.IsVehicleGroup())
		{
			if(!spawnpoint.GetIsSpawned())
			{
				spawnpoint.SpawnPatrol();
				spawnpoint.SetIsPaused(false);
				spawnpoint.ActivateGroup();
				IEntity aiParent = spawnpoint.GetOwner().GetParent();
				SCR_AmbientVehicleSpawnPointComponent AVSPC;
				if(aiParent)
					AVSPC = SCR_AmbientVehicleSpawnPointComponent.Cast(aiParent.FindComponent(SCR_AmbientVehicleSpawnPointComponent));
				Vehicle spawnedVehicle;
				if(AVSPC)
				{
					spawnedVehicle = AVSPC.GetSpawnedVehicle();
					if(spawnedVehicle)
					{
						BaseCompartmentManagerComponent slotCompMan = BaseCompartmentManagerComponent.Cast(spawnedVehicle.FindComponent(BaseCompartmentManagerComponent));
						array<BaseCompartmentSlot> vehicleCompartments = new array<BaseCompartmentSlot>;
						slotCompMan.GetCompartments(vehicleCompartments);
						SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
						group.SetMaxMembers(vehicleCompartments.Count());
						group.SetMaxUnitsToSpawn(vehicleCompartments.Count());
						group.SetNumberOfMembersToSpawn(vehicleCompartments.Count());
					}
				}
				GetGame().GetCallqueue().CallLater(TeleportToVehicle,10000,false,spawnpoint);
			}
			else
			{
				CheckVehicleFuelConsumption(spawnpoint);
			}
			return;
		}
		
		SCR_GameModeCampaign campaign = SCR_GameModeCampaign.GetInstance();
		SCR_CampaignMilitaryBaseManager baseManager = campaign.GetBaseManager();
		SCR_CampaignMilitaryBaseComponent nearestBase;
		bool inRadioRange;
		
		if(!factionProcessed)
		{
			array<Faction> factionList = {};
			GetGame().GetFactionManager().GetFactionsList(factionList);
			foreach(Faction faction: factionList)
			{
				SCR_CampaignFaction usFaction = SCR_CampaignFaction.Cast(faction);
				if(usFaction && usFaction.GetFactionKey() == "US")
				{
					hqBase = usFaction.GetMainBase();
					break;
				}
			}
			factionProcessed = true;
		}
		nearestBase = baseManager.FindClosestBase(spawnpoint.GetOwner().GetOrigin());
		inRadioRange = hqBase.CanReachByRadio(nearestBase.GetOwner());	//new bit
/*		if(!inRadioRange)
		{
			spawnpoint.DeactivateGroup();
			spawnpoint.DespawnPatrol();
			spawnpoint.SetIsSpawned(false);
		}
*/		
/* << Gramps added */		

		bool playersNear;
		bool playersVeryNear;
		bool playersFar = true;
		vector location = spawnpoint.GetOwner().GetOrigin();
		int distance;
		
		// Mike Added, statically sets spawn and despawn distance. Number is in Meters Squared, take the square root of the number to get the value in Meters.
		m_iSpawnDistanceSq = 185000;
		m_iDespawnDistanceSq = 335000;
		// Define if any player is close enough to spawn or if all players are far enough to despawn
		foreach (IEntity player : m_aPlayers)
		{
			if (!player)
				continue;

			distance = vector.DistanceSq(player.GetOrigin(), location);

			if (distance > m_iDespawnDistanceSq)
				continue;

			playersFar = false;

			if (distance > m_iSpawnDistanceSq)
				continue;

			playersNear = true;

			if (distance > SPAWN_RADIUS_BLOCK_SQ)
				continue;

			playersVeryNear = true;
			break;
		}

		bool isAIOverLimit;
		AIWorld aiWorld = GetGame().GetAIWorld();

		if (aiWorld)
		{
			int maxChars = aiWorld.GetLimitOfActiveAIs();

			if (maxChars <= 0)
				isAIOverLimit = true;
			else
				isAIOverLimit = ((float)aiWorld.GetCurrentNumOfActiveAIs() / (float)maxChars) > spawnpoint.GetAILimitThreshold();
		}

		if (playersNear && !spawnpoint.GetIsSpawned() && !playersVeryNear/* && inRadioRange*/)
		{
/*			if(inRadioRange)
			{
*/				spawnpoint.SpawnPatrol();
				spawnpoint.SetIsPaused(false);
/*			} else {
				spawnpoint.SetIsPaused(true);
			}
*/		}
//		Print("[CPR] in radio range? " + inRadioRange + ", base: " + nearestBase.ToString());
		
		if (playersNear && !spawnpoint.GetIsPaused() && !spawnpoint.IsGroupActive())
		{
			// Do not spawn the patrol if the AI threshold setting has been reached
			if (isAIOverLimit/* || !inRadioRange*/)
			{
				spawnpoint.SetIsPaused(true);	// Make sure a patrol is not spawned too close to players when AI limit suddenly allows spawning of this group
//				Print("[CPR] in radio range? " + inRadioRange + ", base: " + nearestBase.ToString());
				return;
			}

			spawnpoint.ActivateGroup();
			return;
		}
		
		// Gramps added this check to prevent ai capturing a point from despawning when players leave the area of a point under attack
		bool capturing = false;
		if (spawnpoint.GetIsSpawned() && spawnpoint.IsGroupActive())
		{
			SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
			if(group)
			{
				nearestBase = baseManager.FindClosestBase(spawnpoint.GetOwner().GetOrigin());
				if(nearestBase.IsBeingCaptured())
				{
					vector basePos = nearestBase.GetOwner().GetOrigin();
					int dist = vector.Distance(group.GetOrigin(), basePos);
					if(dist < nearestBase.GetRadius())	capturing = true;	// Only despawn ai groups outside of base radius
				}
			}
		}
		// End Gramps edit
		
		// Delay is used so dying players don't see the despawn happen
		if (spawnpoint.GetIsSpawned() && playersFar && spawnpoint.IsGroupActive() /*Gramps added>>*/&& !spawnpoint.IsVehicleGroup() && !capturing)
		{
			WorldTimestamp despawnT = spawnpoint.GetDespawnTimer();

			if (despawnT == 0)
				spawnpoint.SetDespawnTimer(currentTime.PlusMilliseconds(DESPAWN_TIMEOUT));
			else if (currentTime.Greater(despawnT)){
				spawnpoint.DeactivateGroup();
				spawnpoint.DespawnPatrol();
			}
		}
		else
		{
			spawnpoint.SetDespawnTimer(null);
		}
	}

/* Gramps added - Check for resetting vehicle fuel consumption when its AI group dies	>> */
	void CheckVehicleFuelConsumption(SCR_AmbientPatrolSpawnPointComponent spawnpoint)
	{
		SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
		if(!group && !spawnpoint.HasBeenEliminated())
		{
			spawnpoint.HasBeenEliminated() = true;
			IEntity aiParent = spawnpoint.GetOwner().GetParent();
			SCR_AmbientVehicleSpawnPointComponent AVSPC;
			if(aiParent)
				AVSPC = SCR_AmbientVehicleSpawnPointComponent.Cast(aiParent.FindComponent(SCR_AmbientVehicleSpawnPointComponent));
			Vehicle spawnedVehicle;
			if(AVSPC)
			{
				spawnedVehicle = AVSPC.GetSpawnedVehicle();
				if(spawnedVehicle)
				{
					SCR_FuelConsumptionComponent fuelComp = SCR_FuelConsumptionComponent.Cast(spawnedVehicle.FindComponent(SCR_FuelConsumptionComponent));
					if(fuelComp)
					{
						SCR_FuelConsumptionComponentClass m_ComponentData = SCR_FuelConsumptionComponentClass.Cast(fuelComp.GetComponentData(spawnedVehicle));
						m_ComponentData.m_fFuelConsumption = 8;
					}
				}
			}
		}
	}

/* Gramps added - TP to parent vehicle from vehicle spawnpoint >> */		
	void TeleportToVehicle(SCR_AmbientPatrolSpawnPointComponent spawnpoint)
	{
		SCR_AIGroup group = spawnpoint.GetSpawnedGroup();
		if(group)
		{
			group.SetIsVehiclePatrol(true);
			array<AIAgent> agents = {};
			group.GetAgents(agents);
			if(agents)
			{
				IEntity aiParent = spawnpoint.GetOwner().GetParent();
				SCR_AmbientVehicleSpawnPointComponent AVSPC;
				if(aiParent)
					AVSPC = SCR_AmbientVehicleSpawnPointComponent.Cast(aiParent.FindComponent(SCR_AmbientVehicleSpawnPointComponent));
				Vehicle spawnedVehicle;
				if(AVSPC)
				{
					spawnedVehicle = AVSPC.GetSpawnedVehicle();
					if(spawnedVehicle)
					{
						SCR_FuelConsumptionComponent fuelComp = SCR_FuelConsumptionComponent.Cast(spawnedVehicle.FindComponent(SCR_FuelConsumptionComponent));
						if(fuelComp)
						{
							SCR_FuelConsumptionComponentClass m_ComponentData = SCR_FuelConsumptionComponentClass.Cast(fuelComp.GetComponentData(spawnedVehicle));
							m_ComponentData.m_fFuelConsumption = 0;
							AICarMovementComponent ACMC = AICarMovementComponent.Cast(spawnedVehicle.FindComponent(AICarMovementComponent));
							ACMC.SetCruiseSpeed(27);
						}
						array<string> groupVehicles = {};
						string vehicle = spawnedVehicle.ToString();
						groupVehicles.Insert(vehicle);
						group.SetGroupVehicles(groupVehicles);
						
						BaseCompartmentManagerComponent slotCompMan = BaseCompartmentManagerComponent.Cast(spawnedVehicle.FindComponent(BaseCompartmentManagerComponent));
						array<BaseCompartmentSlot> vehicleCompartments = new array<BaseCompartmentSlot>;
						slotCompMan.GetCompartments(vehicleCompartments);
						for(int j = 0; j < agents.Count(); j++)
						{
							AIAgent member = agents[j];
							if(member)
							{
								SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(member.GetControlledEntity());
								CompartmentAccessComponent CAComp = CompartmentAccessComponent.Cast(character.FindComponent(CompartmentAccessComponent));
								if(CAComp && character && !character.IsInVehicle() && j < vehicleCompartments.Count())
								{
									BaseCompartmentSlot slot = vehicleCompartments[j];
									if(slot)
										CAComp.GetInVehicle(spawnedVehicle, slot, true, -1, ECloseDoorAfterActions.INVALID, false);
								}
								else
								{
									group.RemoveAgent(member);
									RplComponent.DeleteRplEntity(member.GetControlledEntity().GetRootParent(), false);
								}
							}
						}
					}
				}
			}
		}
	}
}
