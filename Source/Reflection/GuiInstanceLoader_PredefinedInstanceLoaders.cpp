#include "GuiInstanceLoader.h"
#include "GuiInstanceHelperTypes.h"
#include "TypeDescriptors/GuiReflectionTemplates.h"

namespace vl
{
	namespace presentation
	{
		using namespace collections;
		using namespace reflection::description;
		using namespace controls;
		using namespace compositions;
		using namespace theme;
		using namespace helper_types;
		
		using namespace elements;
		using namespace compositions;
		using namespace controls;
		using namespace templates;

		using namespace workflow;
		using namespace workflow::analyzer;

#ifndef VCZH_DEBUG_NO_REFLECTION

/***********************************************************************
GuiVrtualTypeInstanceLoader
***********************************************************************/

		template<typename TControl, typename TControlStyle, typename TTemplate>
		class GuiTemplateControlInstanceLoader : public Object, public IGuiInstanceLoader
		{
			using ArgumentRawFunctionType	= Ptr<WfExpression>();
			using InitRawFunctionType		= void(const WString&, Ptr<WfBlockStatement>);
			using ArgumentFunctionType		= Func<ArgumentRawFunctionType>;
			using InitFunctionType			= Func<InitRawFunctionType>;
		protected:
			GlobalStringKey								typeName;
			WString										styleMethod;
			ArgumentFunctionType						argumentFunction;
			InitFunctionType							initFunction;

		public:

			static Ptr<WfExpression> CreateIThemeCall(const WString& method)
			{
				auto refPresentation = MakePtr<WfTopQualifiedExpression>();
				refPresentation->name.value = L"presentation";

				auto refTheme = MakePtr<WfMemberExpression>();
				refTheme->parent = refPresentation;
				refTheme->name.value = L"theme";

				auto refITheme = MakePtr<WfMemberExpression>();
				refITheme->parent = refTheme;
				refITheme->name.value = L"ITheme";

				auto refStyleMethod = MakePtr<WfMemberExpression>();
				refStyleMethod->parent = refITheme;
				refStyleMethod->name.value = method;

				auto createStyle = MakePtr<WfCallExpression>();
				createStyle->function = refStyleMethod;
				return createStyle;
			}

			static Ptr<WfExpression> CreateStyleMethodArgument(const WString& method)
			{
				if (indexControlTemplate == -1)
				{
					auto refControlStyle = MakePtr<WfReferenceExpression>();
					refControlStyle->name.value = L"<controlStyle>";

					auto refCreateArgument = MakePtr<WfMemberExpression>();
					refCreateArgument->parent = refControlStyle;
					refCreateArgument->name.value = L"CreateArgument";

					auto call = MakePtr<WfCallExpression>();
					call->function = refCreateArgument;

					createControl->arguments.Add(call);
				}
				else
				{
					createControl->arguments.Add(CreateIThemeCall(method));
				}
			}
		public:
			GuiTemplateControlInstanceLoader(const WString& _typeName, const WString& _styleMethod)
				:typeName(GlobalStringKey::Get(_typeName))
				, styleMethod(_styleMethod)
			{
			}

			GuiTemplateControlInstanceLoader(const WString& _typeName, const WString& _styleMethod, WString argumentStyleMethod)
				:typeName(GlobalStringKey::Get(_typeName))
				, styleMethod(_styleMethod)
				, argumentFunction([argumentStyleMethod](){return CreateStyleMethodArgument(argumentStyleMethod);})
			{
			}

			GuiTemplateControlInstanceLoader(const WString& _typeName, const WString& _styleMethod, ArgumentRawFunctionType* _argumentFunction)
				:typeName(GlobalStringKey::Get(_typeName))
				, styleMethod(_styleMethod)
				, argumentFunction(_argumentFunction)
			{
			}

			GuiTemplateControlInstanceLoader(const WString& _typeName, const WString& _styleMethod, InitRawFunctionType* _initFunction)
				:typeName(GlobalStringKey::Get(_typeName))
				, styleMethod(_styleMethod)
				, initFunction(_initFunction)
			{
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			void GetConstructorParameters(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::_ControlTemplate);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::_ControlTemplate)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				return 0;
			}

			bool CanCreate(const TypeInfo& typeInfo)override
			{
				return typeName == typeInfo.typeName;
			}

			Ptr<workflow::WfStatement> CreateInstance(const TypeInfo& typeInfo, GlobalStringKey variableName, ArgumentMap& arguments, collections::List<WString>& errors)override
			{
				CHECK_ERROR(typeName == typeInfo.typeName, L"GuiTemplateControlInstanceLoader::CreateInstance# Wrong type info is provided.");
				vint indexControlTemplate = constructorArguments.Keys().IndexOf(GlobalStringKey::_ControlTemplate);

				Ptr<WfExpression> createStyleExpr;
				if (indexControlTemplate == -1)
				{
					createStyleExpr = CreateIThemeCall(styleMethod);
				}
				else
				{
					auto controlTemplateNameExpr = arguments.GetByIndex(indexControlTemplate).Cast<WfStringExpression>();
					if (!controlTemplateNameExpr)
					{
						errors.Add(L"Precompile: The value of contructor parameter \"" + GlobalStringKey::_ControlTemplate.ToString() + L" of type \"" + typeInfo.typeName.ToString() + L"\" should be a constant representing the control template type name.");
						return nullptr;
					}

					auto controlTemplateTd = description::GetTypeDescriptor(controlTemplateNameExpr->value.value);
					if (!controlTemplateTd)
					{
						errors.Add(L"Precompile: Type \"" + controlTemplateNameExpr->value.value + L", which is assigned to contructor parameter \"" + GlobalStringKey::_ControlTemplate.ToString() + L" of type \"" + typeInfo.typeName.ToString() + L"\", does not exist.");
						return nullptr;
					}

					Ptr<ITypeInfo> controlTemplateType;
					{
						auto elementType = MakePtr<TypeInfoImpl>(ITypeInfo::TypeDescriptor);
						elementType->SetTypeDescriptor(controlTemplateTd);

						auto pointerType = MakePtr<TypeInfoImpl>(ITypeInfo::RawPtr);
						pointerType->SetElementType(elementType);

						controlTemplateType = pointerType;
					}
					auto factoryType = TypeInfoRetriver<Ptr<GuiTemplate::IFactory>>::CreateTypeInfo();
					auto templateType = TypeInfoRetriver<GuiTemplate*>::CreateTypeInfo();

					auto refFactory = MakePtr<WfNewTypeExpression>();
					refFactory->type = GetTypeFromTypeInfo(factoryType.Obj());
					{
						auto funcCreateTemplate = MakePtr<WfFunctionDeclaration>();
						funcCreateTemplate->anonymity = WfFunctionAnonymity::Named;
						funcCreateTemplate->name.value = L"CreateTemplate";
						funcCreateTemplate->returnType = GetTypeFromTypeInfo(templateType.Obj());

						auto argViewModel = MakePtr<WfFunctionArgument>();
						argViewModel->type = GetTypeFromTypeInfo(TypeInfoRetriver<Value>::CreateTypeInfo().Obj());
						argViewModel->name.value = L"<viewModel>";
						funcCreateTemplate->arguments.Add(argViewModel);

						auto block = MakePtr<WfBlockStatement>();
						funcCreateTemplate->statement = block;

						{
							auto createControlTemplate = MakePtr<WfNewTypeExpression>();
							controlControlTemplate->type = GetTypeFromTypeInfo(controlTemplateType.Obj());

							auto varTemplate = MakePtr<WfVariableDeclaration>();
							varTemplate->type = GetTypeFromTypeInfo(templateType.Obj());
							varTemplate->name.value = L"<template>";
							varTemplate->expression = createControlTemplate;

							auto varStat = MakePtr<WfVariableStatement>();
							varStat->variable = varTemplate;
							block->statements.Add(varStat);
						}
						{
							auto refTemplate = MakePtr<WfReferenceExpression>();
							refTemplate->name.value = L"<template>";

							auto returnStat = MakePtr<WfReturnStatement>();
							returnStat->expression = refTemplate;
							block->statements.Add(returnStat);
						}

						refFactory->functions.Add(funcCreateTemplate);
					}

					auto controlType = TypeInfoRetriver<TControl*>::CreateTypeInfo();
					auto styleType = TypeInfoRetriver<TControlStyle*>::CreateTypeInfo();

					auto createStyle = MakePtr<WfNewTypeExpression>();
					createStyle->type = GetTypeFromTypeInfo(styleType.Obj());
					createStyle->arguments.Add(refFactory);
					createStyleExpr = createStyle;
				}
				
				auto block = MakePtr<WfBlockStatement>();
				{
					auto varTemplate = MakePtr<WfVariableDeclaration>();
					varTemplate->name.value = L"<controlStyle>";
					varTemplate->expression = createStyleExpr;

					auto varStat = MakePtr<WfVariableStatement>();
					varStat->variable = varTemplate;
					block->statements.Add(varStat);
				}
				{
					auto controlType = TypeInfoRetriver<TControl*>::CreateTypeInfo();

					auto createControl = MakePtr<WfNewTypeExpression>();
					createControl->type = GetTypeFromTypeInfo(controlType.Obj());
					{
						auto refControlStyle = MakePtr<WfReferenceExpression>();
						refControlStyle->name.value = L"<controlStyle>";

						createControl->arguments.Add(refControlStyle);
					}

					if (argumentFunction != L"")
					{
						createControl->arguments.Add(argumentFunction());
					}

					auto refVariable = MakePtr<WfReferenceExpression>();
					refVariable->name.value = variableName.ToString();

					auto assignExpr = MakePtr<WfBinaryExpression>();
					assignExpr->op = WfBinaryOperator::Assign;
					assignExpr->first = refVariable;
					assignExpr->second = createControl;

					auto assignStat = MakePtr<WfExpressionStatement>();
					assignStat->expression = assignExpr;
					block->statements.Add(assignStat);
				}

				if (initFunction)
				{
					initFunction(variableName.ToString(), block);
				}
				return block;
			}
		};

/***********************************************************************
GuiControlInstanceLoader
***********************************************************************/

		class GuiControlInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey					typeName;

		public:
			GuiControlInstanceLoader()
			{
				typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiControl>()->GetTypeName());
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					auto info = GuiInstancePropertyInfo::Collection();
					info->acceptableTypes.Add(description::GetTypeDescriptor<GuiControl>());
					info->acceptableTypes.Add(description::GetTypeDescriptor<GuiGraphicsComposition>());
					if (propertyInfo.typeInfo.typeDescriptor->CanConvertTo(description::GetTypeDescriptor<GuiInstanceRootObject>()))
					{
						info->acceptableTypes.Add(description::GetTypeDescriptor<GuiComponent>());
					}
					return info;
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiInstanceRootObject*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto component = dynamic_cast<GuiComponent*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->AddComponent(component);
							return true;
						}
						else if (auto controlHost = dynamic_cast<GuiControlHost*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->AddComponent(new GuiObjectComponent<GuiControlHost>(controlHost));
							return true;
						}
					}
				}
				if (auto container = dynamic_cast<GuiControl*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto control = dynamic_cast<GuiControl*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->AddChild(control);
							return true;
						}
						else if (auto composition = dynamic_cast<GuiGraphicsComposition*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->GetContainerComposition()->AddChild(composition);
							return true;
						}
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiTabInstanceLoader
***********************************************************************/

#define BASE_TYPE GuiTemplateControlInstanceLoader<GuiTab, GuiTabTemplate_StyleProvider, GuiTabTemplate>
		class GuiTabInstanceLoader : public BASE_TYPE
		{
		public:
			GuiTabInstanceLoader()
				:BASE_TYPE(description::GetTypeDescriptor<GuiTab>()->GetTypeName(), L"CreateTabStyle")
			{
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					return GuiInstancePropertyInfo::CollectionWithParent(description::GetTypeDescriptor<GuiTabPage>());
				}
				return BASE_TYPE::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiTab*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto tabPage = dynamic_cast<GuiTabPage*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->CreatePage(tabPage);
							return true;
						}
					}
				}
				return false;
			}
		};
#undef BASE_TYPE

/***********************************************************************
GuiTabPageInstanceLoader
***********************************************************************/

		class GuiTabPageInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey					typeName;

		public:
			GuiTabPageInstanceLoader()
			{
				typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiTabPage>()->GetTypeName());
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					auto info = GuiInstancePropertyInfo::Collection();
					info->acceptableTypes.Add(description::GetTypeDescriptor<GuiControl>());
					info->acceptableTypes.Add(description::GetTypeDescriptor<GuiGraphicsComposition>());
					return info;
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiTabPage*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto control = dynamic_cast<GuiControl*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->GetContainerComposition()->AddChild(control->GetBoundsComposition());
							return true;
						}
						else if (auto composition = dynamic_cast<GuiGraphicsComposition*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->GetContainerComposition()->AddChild(composition);
							return true;
						}
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiToolstripMenuInstanceLoader
***********************************************************************/

#define BASE_TYPE GuiTemplateControlInstanceLoader<GuiToolstripMenu, GuiMenuTemplate_StyleProvider, GuiMenuTemplate>
		class GuiToolstripMenuInstanceLoader : public BASE_TYPE
		{
		public:
			static Ptr<WfExpression> ArgumentFunction()
			{
				auto expr = MakePtr<WfLiteralExpression>();
				expr->value = WfLiteralValue::Null;
				return expr;
			}
		public:
			GuiToolstripMenuInstanceLoader()
				:BASE_TYPE(description::GetTypeDescriptor<GuiToolstripMenu>()->GetTypeName(), L"CreateMenuStyle", ArgumentFunction)
			{
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					return GuiInstancePropertyInfo::CollectionWithParent(description::GetTypeDescriptor<GuiControl>());
				}
				return BASE_TYPE::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiToolstripMenu*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto control = dynamic_cast<GuiControl*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->GetToolstripItems().Add(control);
							return true;
						}
					}
				}
				return false;
			}
		};
#undef BASE_TYPE

/***********************************************************************
GuiToolstripMenuBarInstanceLoader
***********************************************************************/

#define BASE_TYPE GuiTemplateControlInstanceLoader<GuiToolstripMenuBar, GuiControlTemplate_StyleProvider, GuiControlTemplate>
		class GuiToolstripMenuBarInstanceLoader : public BASE_TYPE
		{
		public:
			GuiToolstripMenuBarInstanceLoader()
				:BASE_TYPE(description::GetTypeDescriptor<GuiToolstripMenuBar>()->GetTypeName(), L"CreateMenuBarStyle")
			{
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					return GuiInstancePropertyInfo::CollectionWithParent(description::GetTypeDescriptor<GuiControl>());
				}
				return BASE_TYPE::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiToolstripMenuBar*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto control = dynamic_cast<GuiControl*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->GetToolstripItems().Add(control);
							return true;
						}
					}
				}
				return false;
			}
		};
#undef BASE_TYPE

/***********************************************************************
GuiToolstripToolBarInstanceLoader
***********************************************************************/

#define BASE_TYPE GuiTemplateControlInstanceLoader<GuiToolstripToolBar, GuiControlTemplate_StyleProvider, GuiControlTemplate>
		class GuiToolstripToolBarInstanceLoader : public BASE_TYPE
		{
		public:
			GuiToolstripToolBarInstanceLoader()
				:BASE_TYPE(description::GetTypeDescriptor<GuiToolstripToolBar>()->GetTypeName(), L"CreateToolBarStyle")
			{
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					return GuiInstancePropertyInfo::CollectionWithParent(description::GetTypeDescriptor<GuiControl>());
				}
				return BASE_TYPE::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiToolstripToolBar*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto control = dynamic_cast<GuiControl*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->GetToolstripItems().Add(control);
							return true;
						}
					}
				}
				return false;
			}
		};
#undef BASE_TYPE

/***********************************************************************
GuiToolstripButtonInstanceLoader
***********************************************************************/

#define BASE_TYPE GuiTemplateControlInstanceLoader<GuiToolstripButton, GuiToolstripButtonTemplate_StyleProvider, GuiToolstripButtonTemplate>
		class GuiToolstripButtonInstanceLoader : public BASE_TYPE
		{
		protected:
			GlobalStringKey					_SubMenu;

		public:
			GuiToolstripButtonInstanceLoader()
				:BASE_TYPE(description::GetTypeDescriptor<GuiToolstripButton>()->GetTypeName(), L"CreateToolBarButtonStyle")
			{
				_SubMenu = GlobalStringKey::Get(L"SubMenu");
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(_SubMenu);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == _SubMenu)
				{
					return GuiInstancePropertyInfo::Set(description::GetTypeDescriptor<GuiToolstripMenu>());
				}
				return BASE_TYPE::GetPropertyType(propertyInfo);
			}

			bool GetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiToolstripButton*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == _SubMenu)
					{
						if (!container->GetToolstripSubMenu())
						{
							container->CreateToolstripSubMenu();
						}
						propertyValue.propertyValue = Value::From(container->GetToolstripSubMenu());
						return true;
					}
				}
				return false;
			}
		};
#undef BASE_TYPE

/***********************************************************************
GuiSelectableListControlInstanceLoader
***********************************************************************/

		class GuiSelectableListControlInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey					typeName;

		public:
			GuiSelectableListControlInstanceLoader()
			{
				typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiSelectableListControl>()->GetTypeName());
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::_ItemTemplate);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::_ItemTemplate)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
					return info;
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiSelectableListControl*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::_ItemTemplate)
					{
						auto factory = CreateTemplateFactory(propertyValue.propertyValue.GetText());
						auto styleProvider = new GuiListItemTemplate_ItemStyleProvider(factory);
						container->SetStyleProvider(styleProvider);
						return true;
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiVirtualTreeViewInstanceLoader
***********************************************************************/

		class GuiVirtualTreeViewInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey					typeName;

		public:
			GuiVirtualTreeViewInstanceLoader()
			{
				typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiVirtualTreeView>()->GetTypeName());
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::_ItemTemplate);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::_ItemTemplate)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
					return info;
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiVirtualTreeListControl*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::_ItemTemplate)
					{
						auto factory = CreateTemplateFactory(propertyValue.propertyValue.GetText());
						auto styleProvider = new GuiTreeItemTemplate_ItemStyleProvider(factory);
						container->SetNodeStyleProvider(styleProvider);
						return true;
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiListViewInstanceLoader
***********************************************************************/

		class GuiListViewInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			bool				bindable;
			GlobalStringKey		typeName;
			GlobalStringKey		_View, _IconSize, _ItemSource;

		public:
			GuiListViewInstanceLoader(bool _bindable)
				:bindable(_bindable)
			{
				if (bindable)
				{
					typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiBindableListView>()->GetTypeName());
				}
				else
				{
					typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiListView>()->GetTypeName());
				}
				_View = GlobalStringKey::Get(L"View");
				_IconSize = GlobalStringKey::Get(L"IconSize");
				_ItemSource = GlobalStringKey::Get(L"ItemSource");
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			bool IsCreatable(const TypeInfo& typeInfo)override
			{
				return typeInfo.typeName == GetTypeName();
			}

			description::Value CreateInstance(Ptr<GuiInstanceEnvironment> env, const TypeInfo& typeInfo, collections::Group<GlobalStringKey, description::Value>& constructorArguments)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					Ptr<IValueEnumerable> itemSource;
					ListViewViewType viewType = ListViewViewType::Detail;
					GuiListViewBase::IStyleProvider* styleProvider = 0;
					Size iconSize;
					{
						vint indexItemSource = constructorArguments.Keys().IndexOf(_ItemSource);
						if (indexItemSource != -1)
						{
							itemSource = UnboxValue<Ptr<IValueEnumerable>>(constructorArguments.GetByIndex(indexItemSource)[0]);
						}
						else if (bindable)
						{
							return Value();
						}

						vint indexView = constructorArguments.Keys().IndexOf(_View);
						if (indexView != -1)
						{
							viewType = UnboxValue<ListViewViewType>(constructorArguments.GetByIndex(indexView)[0]);
						}

						vint indexIconSize = constructorArguments.Keys().IndexOf(_IconSize);
						if (indexIconSize != -1)
						{
							iconSize = UnboxValue<Size>(constructorArguments.GetByIndex(indexIconSize)[0]);
						}

						vint indexControlTemplate = constructorArguments.Keys().IndexOf(GlobalStringKey::_ControlTemplate);
						if (indexControlTemplate == -1)
						{
							styleProvider = GetCurrentTheme()->CreateListViewStyle();
						}
						else
						{
							auto factory = CreateTemplateFactory(constructorArguments.GetByIndex(indexControlTemplate)[0].GetText());
							styleProvider = new GuiListViewTemplate_StyleProvider(factory);
						}
					}

					GuiVirtualListView* listView = 0;
					if (bindable)
					{
						listView = new GuiBindableListView(styleProvider, itemSource);
					}
					else
					{
						listView = new GuiListView(styleProvider);
					}
					switch (viewType)
					{
#define VIEW_TYPE_CASE(NAME)\
					case ListViewViewType::NAME:\
						if (iconSize == Size())\
						{\
							listView->ChangeItemStyle(new list::ListView##NAME##ContentProvider);\
						}\
						else\
						{\
							listView->ChangeItemStyle(new list::ListView##NAME##ContentProvider(iconSize, false));\
						}\
						break;\

						VIEW_TYPE_CASE(BigIcon)
						VIEW_TYPE_CASE(SmallIcon)
						VIEW_TYPE_CASE(List)
						VIEW_TYPE_CASE(Tile)
						VIEW_TYPE_CASE(Information)
						VIEW_TYPE_CASE(Detail)

#undef VIEW_TYPE_CASE
					}

					return Value::From(listView);
				}
				return Value();
			}

			void GetConstructorParameters(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					propertyNames.Add(GlobalStringKey::_ControlTemplate);
					propertyNames.Add(_View);
					propertyNames.Add(_IconSize);
					if (bindable)
					{
						propertyNames.Add(_ItemSource);
					}
				}
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::_ControlTemplate)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				else if (propertyInfo.propertyName == _View)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<ListViewViewType>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				else if (propertyInfo.propertyName == _IconSize)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<Size>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				else if (propertyInfo.propertyName == _ItemSource)
				{
					if (bindable)
					{
						auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<IValueEnumerable>());
						info->scope = GuiInstancePropertyInfo::Constructor;
						info->required = true;
						return info;
					}
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}
		};

/***********************************************************************
GuiTreeViewInstanceLoader
***********************************************************************/

		class GuiTreeViewInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			bool				bindable;
			GlobalStringKey		typeName;
			GlobalStringKey		_IconSize, _ItemSource, _Nodes;

		public:
			GuiTreeViewInstanceLoader(bool _bindable)
				:bindable(_bindable)
			{
				if (bindable)
				{
					typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiBindableTreeView>()->GetTypeName());
				}
				else
				{
					typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiTreeView>()->GetTypeName());
				}
				_IconSize = GlobalStringKey::Get(L"IconSize");
				_ItemSource = GlobalStringKey::Get(L"ItemSource");
				_Nodes = GlobalStringKey::Get(L"Nodes");
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			bool IsCreatable(const TypeInfo& typeInfo)override
			{
				return typeInfo.typeName == GetTypeName();
			}

			description::Value CreateInstance(Ptr<GuiInstanceEnvironment> env, const TypeInfo& typeInfo, collections::Group<GlobalStringKey, description::Value>& constructorArguments)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					vint indexItemSource = constructorArguments.Keys().IndexOf(_ItemSource);
					GuiVirtualTreeView::IStyleProvider* styleProvider = 0;
					{
						vint indexControlTemplate = constructorArguments.Keys().IndexOf(GlobalStringKey::_ControlTemplate);
						if (indexControlTemplate == -1)
						{
							styleProvider = GetCurrentTheme()->CreateTreeViewStyle();
						}
						else
						{
							auto factory = CreateTemplateFactory(constructorArguments.GetByIndex(indexControlTemplate)[0].GetText());
							styleProvider = new GuiTreeViewTemplate_StyleProvider(factory);
						}
					}

					GuiVirtualTreeView* treeView = 0;
					if (bindable)
					{
						if (indexItemSource == -1)
						{
							return Value();
						}

						auto itemSource = constructorArguments.GetByIndex(indexItemSource)[0];
						treeView = new GuiBindableTreeView(styleProvider, itemSource);
					}
					else
					{
						treeView = new GuiTreeView(styleProvider);
					}

					vint indexIconSize = constructorArguments.Keys().IndexOf(_IconSize);
					if (indexIconSize != -1)
					{
						auto iconSize = UnboxValue<Size>(constructorArguments.GetByIndex(indexIconSize)[0]);
						treeView->SetNodeStyleProvider(new tree::TreeViewNodeItemStyleProvider(iconSize, false));
					}

					return Value::From(treeView);
				}
				return Value();
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				if (!bindable)
				{
					propertyNames.Add(_Nodes);
				}
			}

			void GetConstructorParameters(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					propertyNames.Add(GlobalStringKey::_ControlTemplate);
					propertyNames.Add(_IconSize);
					if (bindable)
					{
						propertyNames.Add(_ItemSource);
					}
				}
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == _Nodes)
				{
					if (!bindable)
					{
						return GuiInstancePropertyInfo::Collection(description::GetTypeDescriptor<tree::MemoryNodeProvider>());
					}
				}
				else if (propertyInfo.propertyName == GlobalStringKey::_ControlTemplate)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				else if (propertyInfo.propertyName == _ItemSource)
				{
					if (bindable)
					{
						auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<Value>());
						info->scope = GuiInstancePropertyInfo::Constructor;
						info->required = true;
						return info;
					}
				}
				else if (propertyInfo.propertyName == _IconSize)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<Size>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiTreeView*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == _Nodes)
					{
						auto item = UnboxValue<Ptr<tree::MemoryNodeProvider>>(propertyValue.propertyValue);
						container->Nodes()->Children().Add(item);
						return true;
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiComboBoxInstanceLoader
***********************************************************************/

		class GuiComboBoxInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey						typeName;
			GlobalStringKey						_ListControl;

		public:
			GuiComboBoxInstanceLoader()
				:typeName(GlobalStringKey::Get(L"presentation::controls::GuiComboBox"))
			{
				_ListControl = GlobalStringKey::Get(L"ListControl");
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			bool IsCreatable(const TypeInfo& typeInfo)override
			{
				return typeInfo.typeName == GetTypeName();
			}

			description::Value CreateInstance(Ptr<GuiInstanceEnvironment> env, const TypeInfo& typeInfo, collections::Group<GlobalStringKey, description::Value>& constructorArguments)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					vint indexListControl = constructorArguments.Keys().IndexOf(_ListControl);
					vint indexControlTemplate = constructorArguments.Keys().IndexOf(GlobalStringKey::_ControlTemplate);
					if (indexListControl != -1)
					{
						Ptr<GuiTemplate::IFactory> factory;
						if (indexControlTemplate != -1)
						{
							factory = CreateTemplateFactory(constructorArguments.GetByIndex(indexControlTemplate)[0].GetText());
						}

						GuiComboBoxBase::IStyleController* styleController = 0;
						if (factory)
						{
							styleController = new GuiComboBoxTemplate_StyleProvider(factory);
						}
						else
						{
							styleController = GetCurrentTheme()->CreateComboBoxStyle();
						}

						auto listControl = UnboxValue<GuiSelectableListControl*>(constructorArguments.GetByIndex(indexListControl)[0]);
						auto comboBox = new GuiComboBoxListControl(styleController, listControl);
						return Value::From(comboBox);
					}
				}
				return Value();
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					propertyNames.Add(_ListControl);
				}
			}

			void GetConstructorParameters(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					propertyNames.Add(GlobalStringKey::_ControlTemplate);
					propertyNames.Add(_ListControl);
				}
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == _ListControl)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<GuiSelectableListControl>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					info->required = true;
					return info;
				}
				else if (propertyInfo.propertyName == GlobalStringKey::_ControlTemplate)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}
		};

/***********************************************************************
GuiBindableTextListInstanceLoader
***********************************************************************/

		class GuiBindableTextListInstanceLoader : public Object, public IGuiInstanceLoader
		{
			typedef Func<list::TextItemStyleProvider::ITextItemStyleProvider*()>		ItemStyleProviderFactory;
		protected:
			GlobalStringKey					typeName;
			ItemStyleProviderFactory		itemStyleProviderFactory;
			GlobalStringKey					_ItemSource;

		public:
			GuiBindableTextListInstanceLoader(const WString& type, const ItemStyleProviderFactory& factory)
				:typeName(GlobalStringKey::Get(L"presentation::controls::GuiBindable" + type + L"TextList"))
				, itemStyleProviderFactory(factory)
			{
				_ItemSource = GlobalStringKey::Get(L"ItemSource");
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			bool IsCreatable(const TypeInfo& typeInfo)override
			{
				return typeInfo.typeName == GetTypeName();
			}

			description::Value CreateInstance(Ptr<GuiInstanceEnvironment> env, const TypeInfo& typeInfo, collections::Group<GlobalStringKey, description::Value>& constructorArguments)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					vint indexItemSource = constructorArguments.Keys().IndexOf(_ItemSource);
					if (indexItemSource != -1)
					{
						GuiTextListTemplate_StyleProvider* styleProvider = 0;
						{
							vint indexControlTemplate = constructorArguments.Keys().IndexOf(GlobalStringKey::_ControlTemplate);
							if (indexControlTemplate != -1)
							{
								auto factory = CreateTemplateFactory(constructorArguments.GetByIndex(indexControlTemplate)[0].GetText());
								styleProvider = new GuiTextListTemplate_StyleProvider(factory);
							}
						}

						auto itemSource = UnboxValue<Ptr<IValueEnumerable>>(constructorArguments.GetByIndex(indexItemSource)[0]);
						GuiBindableTextList* control = 0;
						if (styleProvider)
						{
							control = new GuiBindableTextList(styleProvider, styleProvider->CreateArgument(), itemSource);
						}
						else
						{
							control = new GuiBindableTextList(GetCurrentTheme()->CreateTextListStyle(), itemStyleProviderFactory(), itemSource);
						}
						return Value::From(control);
					}
				}
				return Value();
			}

			void GetConstructorParameters(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					propertyNames.Add(GlobalStringKey::_ControlTemplate);
					propertyNames.Add(_ItemSource);
				}
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::_ControlTemplate)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				if (propertyInfo.propertyName == _ItemSource)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<IValueEnumerable>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					info->required = true;
					return info;
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}
		};

/***********************************************************************
GuiBindableDataColumnInstanceLoader
***********************************************************************/

		class GuiBindableDataColumnInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey		typeName;
			GlobalStringKey		_VisualizerTemplates;
			GlobalStringKey		_EditorTemplate;

		public:
			GuiBindableDataColumnInstanceLoader()
			{
				typeName = GlobalStringKey::Get(description::GetTypeDescriptor<list::BindableDataColumn>()->GetTypeName());
				_VisualizerTemplates = GlobalStringKey::Get(L"VisualizerTemplates");
				_EditorTemplate = GlobalStringKey::Get(L"EditorTemplate");
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(_VisualizerTemplates);
				propertyNames.Add(_EditorTemplate);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == _VisualizerTemplates)
				{
					return GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
				}
				else if (propertyInfo.propertyName == _EditorTemplate)
				{
					return GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<list::BindableDataColumn*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == _VisualizerTemplates)
					{
						List<WString> types;
						SplitBySemicolon(propertyValue.propertyValue.GetText(), types);
						Ptr<list::IDataVisualizerFactory> factory;
						FOREACH(WString, type, types)
						{
							auto templateFactory = CreateTemplateFactory(type);
							if (factory)
							{
								factory = new GuiBindableDataVisualizer::DecoratedFactory(templateFactory, container, factory);
							}
							else
							{
								factory = new GuiBindableDataVisualizer::Factory(templateFactory, container);
							}
						}

						container->SetVisualizerFactory(factory);
						return true;
					}
					else if (propertyValue.propertyName == _EditorTemplate)
					{
						auto templateFactory = CreateTemplateFactory(propertyValue.propertyValue.GetText());
						auto factory = new GuiBindableDataEditor::Factory(templateFactory, container);
						container->SetEditorFactory(factory);
						return true;
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiBindableDataGridInstanceLoader
***********************************************************************/

		class GuiBindableDataGridInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey		typeName;
			GlobalStringKey		_ItemSource;
			GlobalStringKey		_ViewModelContext;
			GlobalStringKey		_Columns;

		public:
			GuiBindableDataGridInstanceLoader()
			{
				typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiBindableDataGrid>()->GetTypeName());
				_ItemSource = GlobalStringKey::Get(L"ItemSource");
				_ViewModelContext = GlobalStringKey::Get(L"ViewModelContext");
				_Columns = GlobalStringKey::Get(L"Columns");
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			bool IsCreatable(const TypeInfo& typeInfo)override
			{
				return typeInfo.typeName == GetTypeName();
			}

			description::Value CreateInstance(Ptr<GuiInstanceEnvironment> env, const TypeInfo& typeInfo, collections::Group<GlobalStringKey, description::Value>& constructorArguments)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					vint indexItemSource = constructorArguments.Keys().IndexOf(_ItemSource);	
					if (indexItemSource == -1)
					{
						return Value();
					}

					GuiBindableDataGrid::IStyleProvider* styleProvider = 0;
					vint indexControlTemplate = constructorArguments.Keys().IndexOf(GlobalStringKey::_ControlTemplate);
					if (indexControlTemplate == -1)
					{
						styleProvider = GetCurrentTheme()->CreateListViewStyle();
					}
					else
					{
						auto factory = CreateTemplateFactory(constructorArguments.GetByIndex(indexControlTemplate)[0].GetText());
						styleProvider = new GuiListViewTemplate_StyleProvider(factory);
					}
					
					auto itemSource = UnboxValue<Ptr<IValueEnumerable>>(constructorArguments.GetByIndex(indexItemSource)[0]);

					Value viewModelContext;
					vint indexViewModelContext = constructorArguments.Keys().IndexOf(_ViewModelContext);
					if (indexViewModelContext != -1)
					{
						viewModelContext = constructorArguments.GetByIndex(indexViewModelContext)[0];
					}

					auto dataGrid = new GuiBindableDataGrid(styleProvider, itemSource, viewModelContext);
					return Value::From(dataGrid);
				}
				return Value();
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(_Columns);
			}

			void GetConstructorParameters(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					propertyNames.Add(GlobalStringKey::_ControlTemplate);
					propertyNames.Add(_ItemSource);
					propertyNames.Add(_ViewModelContext);
				}
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == _Columns)
				{
					return GuiInstancePropertyInfo::Collection(description::GetTypeDescriptor<list::BindableDataColumn>());
				}
				else if (propertyInfo.propertyName == GlobalStringKey::_ControlTemplate)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				else if (propertyInfo.propertyName == _ItemSource)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<IValueEnumerable>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					info->required = true;
					return info;
				}
				else if (propertyInfo.propertyName == _ViewModelContext)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<Value>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiBindableDataGrid*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == _Columns)
					{
						auto column = UnboxValue<Ptr<list::BindableDataColumn>>(propertyValue.propertyValue);
						container->AddBindableColumn(column);
						return true;
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiDocumentItemInstanceLoader
***********************************************************************/

		class GuiDocumentItemInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey					typeName;
			GlobalStringKey					_Name;

		public:
			GuiDocumentItemInstanceLoader()
			{
				typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiDocumentItem>()->GetTypeName());
				_Name = GlobalStringKey::Get(L"Name");
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			bool IsCreatable(const TypeInfo& typeInfo)override
			{
				return typeName == typeInfo.typeName;
			}

			description::Value CreateInstance(Ptr<GuiInstanceEnvironment> env, const TypeInfo& typeInfo, collections::Group<GlobalStringKey, description::Value>& constructorArguments)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					vint indexName = constructorArguments.Keys().IndexOf(_Name);	
					if (indexName == -1)
					{
						return Value();
					}

					auto name = UnboxValue<WString>(constructorArguments.GetByIndex(indexName)[0]);
					auto item = MakePtr<GuiDocumentItem>(name);
					return Value::From(item);
				}
				return Value();
			}

			void GetConstructorParameters(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					propertyNames.Add(_Name);
				}
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					auto info = GuiInstancePropertyInfo::Collection();
					info->acceptableTypes.Add(description::GetTypeDescriptor<GuiControl>());
					info->acceptableTypes.Add(description::GetTypeDescriptor<GuiGraphicsComposition>());
					return info;
				}
				else if (propertyInfo.propertyName == _Name)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiDocumentItem*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto control = dynamic_cast<GuiControl*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->GetContainer()->AddChild(control->GetBoundsComposition());
							return true;
						}
						else if (auto composition = dynamic_cast<GuiGraphicsComposition*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->GetContainer()->AddChild(composition);
							return true;
						}
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiDocumentCommonInterfaceInstanceLoader
***********************************************************************/

		class GuiDocumentCommonInterfaceInstanceLoader : public Object, public IGuiInstanceLoader
		{
		public:
			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					return GuiInstancePropertyInfo::CollectionWithParent(description::GetTypeDescriptor<GuiDocumentItem>());
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiDocumentCommonInterface*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto item = propertyValue.propertyValue.GetSharedPtr().Cast<GuiDocumentItem>())
						{
							container->AddDocumentItem(item);
							return true;
						}
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiDocumentViewerInstanceLoader
***********************************************************************/

#define BASE_TYPE GuiTemplateControlInstanceLoader<GuiDocumentViewer, GuiDocumentViewerTemplate_StyleProvider, GuiDocumentViewerTemplate>
		class GuiDocumentViewerInstanceLoader : public BASE_TYPE
		{
		public:
			GuiDocumentViewerInstanceLoader()
				:BASE_TYPE(description::GetTypeDescriptor<GuiDocumentViewer>()->GetTypeName(), L"CreateDocumentViewerStyle")
			{
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					return GuiInstancePropertyInfo::CollectionWithParent(description::GetTypeDescriptor<GuiDocumentItem>());
				}
				return BASE_TYPE::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiDocumentCommonInterface*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto item = propertyValue.propertyValue.GetSharedPtr().Cast<GuiDocumentItem>())
						{
							container->AddDocumentItem(item);
							return true;
						}
					}
				}
				return false;
			}
		};
#undef BASE_TYPE

/***********************************************************************
GuiDocumentLabelInstanceLoader
***********************************************************************/
		
#define BASE_TYPE GuiTemplateControlInstanceLoader<GuiDocumentLabel, GuiDocumentLabelTemplate_StyleProvider, GuiDocumentLabelTemplate>
		class GuiDocumentLabelInstanceLoader : public BASE_TYPE
		{
		public:
			GuiDocumentLabelInstanceLoader()
				:BASE_TYPE(description::GetTypeDescriptor<GuiDocumentLabel>()->GetTypeName(), L"CreateDocumentLabelStyle")
			{
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					return GuiInstancePropertyInfo::CollectionWithParent(description::GetTypeDescriptor<GuiDocumentItem>());
				}
				return BASE_TYPE::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiDocumentCommonInterface*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto item = propertyValue.propertyValue.GetSharedPtr().Cast<GuiDocumentItem>())
						{
							container->AddDocumentItem(item);
							return true;
						}
					}
				}
				return false;
			}
		};
#undef BASE_TYPE

/***********************************************************************
GuiAxisInstanceLoader
***********************************************************************/

		class GuiAxisInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey					typeName;
			GlobalStringKey					_AxisDirection;

		public:
			GuiAxisInstanceLoader()
			{
				typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiAxis>()->GetTypeName());
				_AxisDirection = GlobalStringKey::Get(L"AxisDirection");
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			bool IsCreatable(const TypeInfo& typeInfo)override
			{
				return typeName == typeInfo.typeName;
			}

			description::Value CreateInstance(Ptr<GuiInstanceEnvironment> env, const TypeInfo& typeInfo, collections::Group<GlobalStringKey, description::Value>& constructorArguments)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					vint indexAxisDirection = constructorArguments.Keys().IndexOf(_AxisDirection);	
					if (indexAxisDirection == -1)
					{
						return Value();
					}

					auto axisDirection = UnboxValue<AxisDirection>(constructorArguments.GetByIndex(indexAxisDirection)[0]);
					auto axis = new GuiAxis(axisDirection);
					return Value::From(axis);
				}
				return Value();
			}

			void GetConstructorParameters(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					propertyNames.Add(_AxisDirection);
				}
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == _AxisDirection)
				{
					auto info = GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<AxisDirection>());
					info->scope = GuiInstancePropertyInfo::Constructor;
					return info;
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}
		};

/***********************************************************************
GuiCompositionInstanceLoader
***********************************************************************/

		class GuiCompositionInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey					typeName;

		public:
			GuiCompositionInstanceLoader()
			{
				typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiGraphicsComposition>()->GetTypeName());
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					auto info = GuiInstancePropertyInfo::Collection();
					info->acceptableTypes.Add(description::GetTypeDescriptor<GuiControl>());
					info->acceptableTypes.Add(description::GetTypeDescriptor<GuiGraphicsComposition>());
					info->acceptableTypes.Add(description::GetTypeDescriptor<IGuiGraphicsElement>());
					return info;
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiGraphicsComposition*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						if (auto control = dynamic_cast<GuiControl*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->AddChild(control->GetBoundsComposition());
							return true;
						}
						else if(auto composition = dynamic_cast<GuiGraphicsComposition*>(propertyValue.propertyValue.GetRawPtr()))
						{
							container->AddChild(composition);
							return true;
						}
						else if (Ptr<IGuiGraphicsElement> element = propertyValue.propertyValue.GetSharedPtr().Cast<IGuiGraphicsElement>())
						{
							container->SetOwnedElement(element);
							return true;
						}
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiTableCompositionInstanceLoader
***********************************************************************/

		class GuiTableCompositionInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey					typeName;
			GlobalStringKey					_Rows, _Columns;

		public:
			GuiTableCompositionInstanceLoader()
			{
				typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiTableComposition>()->GetTypeName());
				_Rows = GlobalStringKey::Get(L"Rows");
				_Columns = GlobalStringKey::Get(L"Columns");
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(_Rows);
				propertyNames.Add(_Columns);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == _Rows || propertyInfo.propertyName == _Columns)
				{
					return GuiInstancePropertyInfo::Array(description::GetTypeDescriptor<GuiCellOption>());
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiTableComposition*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == _Rows)
					{
						List<GuiCellOption> options;
						CopyFrom(options, GetLazyList<GuiCellOption>(UnboxValue<Ptr<IValueList>>(propertyValue.propertyValue)));
						container->SetRowsAndColumns(options.Count(), container->GetColumns());
						FOREACH_INDEXER(GuiCellOption, option, index, options)
						{
							container->SetRowOption(index, option);
						}
						return true;
					}
					else if (propertyValue.propertyName == _Columns)
					{
						List<GuiCellOption> options;
						CopyFrom(options, GetLazyList<GuiCellOption>(UnboxValue<Ptr<IValueList>>(propertyValue.propertyValue)));
						container->SetRowsAndColumns(container->GetRows(), options.Count());
						FOREACH_INDEXER(GuiCellOption, option, index, options)
						{
							container->SetColumnOption(index, option);
						}
						return true;
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiCellCompositionInstanceLoader
***********************************************************************/

		class GuiCellCompositionInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey					typeName;
			GlobalStringKey					_Site;

		public:
			GuiCellCompositionInstanceLoader()
			{
				typeName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiCellComposition>()->GetTypeName());
				_Site = GlobalStringKey::Get(L"Site");
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(_Site);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == _Site)
				{
					return GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<SiteValue>());
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<GuiCellComposition*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == _Site)
					{
						SiteValue site = UnboxValue<SiteValue>(propertyValue.propertyValue);
						container->SetSite(site.row, site.column, site.rowSpan, site.columnSpan);
						return true;
					}
				}
				return false;
			}
		};

/***********************************************************************
GuiTreeNodeInstanceLoader
***********************************************************************/

		class GuiTreeNodeInstanceLoader : public Object, public IGuiInstanceLoader
		{
		protected:
			GlobalStringKey							typeName;
			GlobalStringKey							_Text, _Image, _Tag;

		public:
			GuiTreeNodeInstanceLoader()
				:typeName(GlobalStringKey::Get(L"presentation::controls::tree::TreeNode"))
			{
				_Text = GlobalStringKey::Get(L"Text");
				_Image = GlobalStringKey::Get(L"Image");
				_Tag = GlobalStringKey::Get(L"Tag");
			}

			GlobalStringKey GetTypeName()override
			{
				return typeName;
			}

			bool IsCreatable(const TypeInfo& typeInfo)override
			{
				return typeInfo.typeName == GetTypeName();
			}

			description::Value CreateInstance(Ptr<GuiInstanceEnvironment> env, const TypeInfo& typeInfo, collections::Group<GlobalStringKey, description::Value>& constructorArguments)override
			{
				if (typeInfo.typeName == GetTypeName())
				{
					Ptr<tree::TreeViewItem> item = new tree::TreeViewItem;
					Ptr<tree::MemoryNodeProvider> node = new tree::MemoryNodeProvider(item);
					return Value::From(node);
				}
				return Value();
			}

			void GetPropertyNames(const TypeInfo& typeInfo, collections::List<GlobalStringKey>& propertyNames)override
			{
				propertyNames.Add(_Text);
				propertyNames.Add(_Image);
				propertyNames.Add(_Tag);
				propertyNames.Add(GlobalStringKey::Empty);
			}

			Ptr<GuiInstancePropertyInfo> GetPropertyType(const PropertyInfo& propertyInfo)override
			{
				if (propertyInfo.propertyName == _Text)
				{
					return GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<WString>());
				}
				else if (propertyInfo.propertyName == _Image)
				{
					return GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<GuiImageData>());
				}
				else if (propertyInfo.propertyName == _Tag)
				{
					return GuiInstancePropertyInfo::Assign(description::GetTypeDescriptor<Value>());
				}
				else if (propertyInfo.propertyName == GlobalStringKey::Empty)
				{
					return GuiInstancePropertyInfo::Collection(description::GetTypeDescriptor<tree::MemoryNodeProvider>());
				}
				return IGuiInstanceLoader::GetPropertyType(propertyInfo);
			}

			bool SetPropertyValue(PropertyValue& propertyValue)override
			{
				if (auto container = dynamic_cast<tree::MemoryNodeProvider*>(propertyValue.instanceValue.GetRawPtr()))
				{
					if (propertyValue.propertyName == _Text)
					{
						if (auto item = container->GetData().Cast<tree::TreeViewItem>())
						{
							item->text = UnboxValue<WString>(propertyValue.propertyValue);
							container->NotifyDataModified();
							return true;
						}
					}
					else if (propertyValue.propertyName == _Image)
					{
						if (auto item = container->GetData().Cast<tree::TreeViewItem>())
						{
							item->image = UnboxValue<Ptr<GuiImageData>>(propertyValue.propertyValue);
							container->NotifyDataModified();
							return true;
						}
					}
					else if (propertyValue.propertyName == _Tag)
					{
						if (auto item = container->GetData().Cast<tree::TreeViewItem>())
						{
							item->tag = propertyValue.propertyValue;
							return true;
						}
					}
					else if (propertyValue.propertyName == GlobalStringKey::Empty)
					{
						auto item = UnboxValue<Ptr<tree::MemoryNodeProvider>>(propertyValue.propertyValue);
						container->Children().Add(item);
						return true;
					}
				}
				return false;
			}
		};

#endif

/***********************************************************************
GuiPredefinedInstanceLoadersPlugin
***********************************************************************/

		Ptr<WfExpression> CreateStandardDataPicker()
		{
			using TControl = GuiDatePicker;
			using TControlStyle = GuiDateComboBoxTemplate_StyleProvider;
			using TTemplate = GuiDatePickerTemplate;

			auto controlType = TypeInfoRetriver<TControl*>::CreateTypeInfo();
			auto createControl = MakePtr<WfNewTypeExpression>();
			createControl->type = GetTypeFromTypeInfo(controlType.Obj());
			createControl->arguments.Add(GuiTemplateControlInstanceLoader<TControl, TControlStyle, TTemplate>::CreateIThemeCall(L"CreateDatePickerStyle"));

			return createControl;
		}

		void InitializeTrackerProgressBar(const WString& variableName, Ptr<WfBlockStatement> block)
		{
			auto refVariable = MakePtr<WfReferenceExpression>();
			refVariable->name.value = variableName;

			auto refSetPageSize = MakePtr<WfMemberExpression>();
			refSetPageSize->parent = refVariable;
			refSetPageSize->name.value = L"SetPageSize";

			auto refZero = MakePtr<WfIntegerExpression>();
			refZero->value.value = L"0";

			auto call = MakePtr<WfCallExpression>();
			call->function = refSetPageSize;
			call->arguments.Add(refZero);

			auto stat = MakePtr<WfExpressionStatement>();
			stat->expression = call;
			block->statements.Add(stat);
		}

		class GuiPredefinedInstanceLoadersPlugin : public Object, public IGuiPlugin
		{
		public:
			void Load()override
			{
			}

			void AfterLoad()override
			{
#ifndef VCZH_DEBUG_NO_REFLECTION
				IGuiInstanceLoaderManager* manager=GetInstanceLoaderManager();

#define ADD_VIRTUAL_TYPE_LOADER(TYPENAME, LOADER)\
	manager->CreateVirtualType(\
		GlobalStringKey::Get(description::GetTypeDescriptor<TYPENAME>()->GetTypeName()),\
		new LOADER\
		)

#define ADD_TEMPLATE_CONTROL(TYPENAME, STYLE_METHOD, TEMPLATE)\
	manager->SetLoader(\
	new GuiTemplateControlInstanceLoader<TYPENAME, TEMPLATE##_StyleProvider, TEMPLATE>(\
			L"presentation::controls::" L ## #TYPENAME,\
			L ## #STYLE_METHOD\
			)\
		)

#define ADD_TEMPLATE_CONTROL_2(TYPENAME, STYLE_METHOD, ARGUMENT_METHOD, TEMPLATE)\
	manager->SetLoader(\
	new GuiTemplateControlInstanceLoader<TYPENAME, TEMPLATE##_StyleProvider, TEMPLATE>(\
			L"presentation::controls::" L ## #TYPENAME,\
			L ## #STYLE_METHOD,\
			L ## #ARGUMENT_METHOD\
			)\
		)

#define ADD_TEMPLATE_CONTROL_3(TYPENAME, STYLE_METHOD, ARGUMENT_FUNCTION, TEMPLATE)\
	manager->SetLoader(\
	new GuiTemplateControlInstanceLoader<TYPENAME, TEMPLATE##_StyleProvider, TEMPLATE>(\
			L"presentation::controls::" L ## #TYPENAME,\
			L ## #STYLE_METHOD,\
			ARGUMENT_FUNCTION\
			)\
		)

#define ADD_VIRTUAL_CONTROL(VIRTUALTYPENAME, TYPENAME, STYLE_METHOD, TEMPLATE)\
	manager->CreateVirtualType(GlobalStringKey::Get(description::GetTypeDescriptor<TYPENAME>()->GetTypeName()),\
	new GuiTemplateControlInstanceLoader<TYPENAME, TEMPLATE##_StyleProvider, TEMPLATE>(\
			L"presentation::controls::Gui" L ## #VIRTUALTYPENAME,\
			L ## #STYLE_METHOD\
			)\
		)

#define ADD_VIRTUAL_CONTROL_2(VIRTUALTYPENAME, TYPENAME, STYLE_METHOD, ARGUMENT_METHOD, TEMPLATE)\
	manager->CreateVirtualType(GlobalStringKey::Get(description::GetTypeDescriptor<TYPENAME>()->GetTypeName()),\
	new GuiTemplateControlInstanceLoader<TYPENAME, TEMPLATE##_StyleProvider, TEMPLATE>(\
			L"presentation::controls::Gui" L ## #VIRTUALTYPENAME,\
			L ## #STYLE_METHOD,\
			L ## #ARGUMENT_METHOD\
			)\
		)

#define ADD_VIRTUAL_CONTROL_F(VIRTUALTYPENAME, TYPENAME, STYLE_METHOD, TEMPLATE, INIT_FUNCTION)\
	manager->CreateVirtualType(GlobalStringKey::Get(description::GetTypeDescriptor<TYPENAME>()->GetTypeName()),\
	new GuiTemplateControlInstanceLoader<TYPENAME, TEMPLATE##_StyleProvider, TEMPLATE>(\
			L"presentation::controls::Gui" L ## #VIRTUALTYPENAME,\
			L ## #STYLE_METHOD,\
			INIT_FUNCTION\
			)\
		)

				manager->SetLoader(new GuiControlInstanceLoader);
				manager->SetLoader(new GuiTabInstanceLoader);						// ControlTemplate
				manager->SetLoader(new GuiTabPageInstanceLoader);
				manager->SetLoader(new GuiToolstripMenuInstanceLoader);				// ControlTemplate
				manager->SetLoader(new GuiToolstripMenuBarInstanceLoader);			// ControlTemplate
				manager->SetLoader(new GuiToolstripToolBarInstanceLoader);			// ControlTemplate
				manager->SetLoader(new GuiToolstripButtonInstanceLoader);			// ControlTemplate
				manager->SetLoader(new GuiSelectableListControlInstanceLoader);		// ItemTemplate
				manager->SetLoader(new GuiVirtualTreeViewInstanceLoader);			// ItemTemplate
				manager->SetLoader(new GuiListViewInstanceLoader(false));			// ControlTemplate
				manager->SetLoader(new GuiTreeViewInstanceLoader(false));			// ControlTemplate
				manager->SetLoader(new GuiBindableTextListInstanceLoader(L"", [](){return GetCurrentTheme()->CreateTextListItemStyle(); }));	// ControlTemplate, ItemSource
				manager->SetLoader(new GuiListViewInstanceLoader(true));			// ControlTemplate, ItemSource
				manager->SetLoader(new GuiTreeViewInstanceLoader(true));			// ControlTemplate, ItemSource
				manager->SetLoader(new GuiBindableDataColumnInstanceLoader);		// VisualizerTemplates, EditorTemplate
				manager->SetLoader(new GuiBindableDataGridInstanceLoader);			// ControlTemplate, ItemSource

				manager->SetLoader(new GuiDocumentItemInstanceLoader);
				manager->SetLoader(new GuiDocumentViewerInstanceLoader);			// ControlTemplate
				manager->SetLoader(new GuiDocumentLabelInstanceLoader);				// ControlTemplate

				manager->SetLoader(new GuiAxisInstanceLoader);
				manager->SetLoader(new GuiCompositionInstanceLoader);
				manager->SetLoader(new GuiTableCompositionInstanceLoader);
				manager->SetLoader(new GuiCellCompositionInstanceLoader);
				
				ADD_VIRTUAL_TYPE_LOADER(GuiComboBoxListControl,						GuiComboBoxInstanceLoader);				// ControlTemplate
				ADD_VIRTUAL_TYPE_LOADER(tree::MemoryNodeProvider,					GuiTreeNodeInstanceLoader);

				ADD_TEMPLATE_CONTROL	(							GuiCustomControl,		CreateCustomControlStyle,											GuiControlTemplate											);
				ADD_TEMPLATE_CONTROL	(							GuiLabel,				CreateLabelStyle,													GuiLabelTemplate											);
				ADD_TEMPLATE_CONTROL	(							GuiButton,				CreateButtonStyle,													GuiButtonTemplate											);
				ADD_TEMPLATE_CONTROL	(							GuiScrollContainer,		CreateScrollContainerStyle,											GuiScrollViewTemplate										);
				ADD_TEMPLATE_CONTROL	(							GuiWindow,				CreateWindowStyle,													GuiWindowTemplate											);
				ADD_TEMPLATE_CONTROL_2	(							GuiTextList,			CreateTextListStyle,				CreateTextListItemStyle,		GuiTextListTemplate											);
				ADD_TEMPLATE_CONTROL	(							GuiMultilineTextBox,	CreateMultilineTextBoxStyle,										GuiMultilineTextBoxTemplate									);
				ADD_TEMPLATE_CONTROL	(							GuiSinglelineTextBox,	CreateTextBoxStyle,													GuiSinglelineTextBoxTemplate								);
				ADD_TEMPLATE_CONTROL	(							GuiDatePicker,			CreateDatePickerStyle,												GuiDatePickerTemplate										);
				ADD_TEMPLATE_CONTROL_3	(							GuiDateComboBox,		CreateComboBoxStyle,				CreateStandardDataPicker,		GuiDateComboBoxTemplate										);
				ADD_TEMPLATE_CONTROL	(							GuiStringGrid,			CreateListViewStyle,												GuiListViewTemplate											);

				ADD_VIRTUAL_CONTROL		(GroupBox,					GuiControl,				CreateGroupBoxStyle,												GuiControlTemplate											);
				ADD_VIRTUAL_CONTROL		(MenuSplitter,				GuiControl,				CreateMenuSplitterStyle,											GuiControlTemplate											);
				ADD_VIRTUAL_CONTROL		(MenuBarButton,				GuiToolstripButton,		CreateMenuBarButtonStyle,											GuiToolstripButtonTemplate									);
				ADD_VIRTUAL_CONTROL		(MenuItemButton,			GuiToolstripButton,		CreateMenuItemButtonStyle,											GuiToolstripButtonTemplate									);
				ADD_VIRTUAL_CONTROL		(ToolstripDropdownButton,	GuiToolstripButton,		CreateToolBarDropdownButtonStyle,									GuiToolstripButtonTemplate									);
				ADD_VIRTUAL_CONTROL		(ToolstripSplitButton,		GuiToolstripButton,		CreateToolBarSplitButtonStyle,										GuiToolstripButtonTemplate									);
				ADD_VIRTUAL_CONTROL		(ToolstripSplitter,			GuiControl,				CreateToolBarSplitterStyle,											GuiControlTemplate											);
				ADD_VIRTUAL_CONTROL		(CheckBox,					GuiSelectableButton,	CreateCheckBoxStyle,												GuiSelectableButtonTemplate									);
				ADD_VIRTUAL_CONTROL		(RadioButton,				GuiSelectableButton,	CreateRadioButtonStyle,												GuiSelectableButtonTemplate									);
				ADD_VIRTUAL_CONTROL		(HScroll,					GuiScroll,				CreateHScrollStyle,													GuiScrollTemplate											);
				ADD_VIRTUAL_CONTROL		(VScroll,					GuiScroll,				CreateVScrollStyle,													GuiScrollTemplate											);
				ADD_VIRTUAL_CONTROL_F	(HTracker,					GuiScroll,				CreateHTrackerStyle,												GuiScrollTemplate,				InitializeTrackerProgressBar);
				ADD_VIRTUAL_CONTROL_F	(VTracker,					GuiScroll,				CreateVTrackerStyle,												GuiScrollTemplate,				InitializeTrackerProgressBar);
				ADD_VIRTUAL_CONTROL_F	(ProgressBar,				GuiScroll,				CreateProgressBarStyle,												GuiScrollTemplate,				InitializeTrackerProgressBar);
				ADD_VIRTUAL_CONTROL_2	(CheckTextList,				GuiTextList,			CreateTextListStyle,				CreateCheckTextListItemStyle,	GuiTextListTemplate											);
				ADD_VIRTUAL_CONTROL_2	(RadioTextList,				GuiTextList,			CreateTextListStyle,				CreateRadioTextListItemStyle,	GuiTextListTemplate											);

				auto bindableTextListName = GlobalStringKey::Get(description::GetTypeDescriptor<GuiBindableTextList>()->GetTypeName());						// ControlTemplate, ItemSource
				manager->CreateVirtualType(bindableTextListName, new GuiBindableTextListInstanceLoader(L"Check", [](){return GetCurrentTheme()->CreateCheckTextListItemStyle(); }));
				manager->CreateVirtualType(bindableTextListName, new GuiBindableTextListInstanceLoader(L"Radio", [](){return GetCurrentTheme()->CreateRadioTextListItemStyle(); }));

#undef ADD_VIRTUAL_TYPE
#undef ADD_VIRTUAL_TYPE_LOADER
#endif
			}

			void Unload()override
			{
			}
		};
		GUI_REGISTER_PLUGIN(GuiPredefinedInstanceLoadersPlugin)
	}
}