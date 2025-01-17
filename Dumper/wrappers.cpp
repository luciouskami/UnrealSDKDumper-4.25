#include <Windows.h>
#include <algorithm>
#include <fmt/core.h>
#include <hash/hash.h>
#include <algorithm>
#include <sstream>
#include "engine.h"
#include "memory.h"
#include "wrappers.h"
#include "ClassSizeFixer.h"
#include "EngineHeaderExport.h"
#include <cassert>

std::pair<bool, uint16> UE_FNameEntry::Info() const {
  auto info = Read<uint16>(object + offsets.FNameEntry.Info);
  auto len = info >> offsets.FNameEntry.LenBit;
  bool wide = (info >> offsets.FNameEntry.WideBit) & 1;
  return {wide, len};
}

std::string UE_FNameEntry::String(bool wide, uint16 len) const {
  if (wide) {
    wchar_t wbuf[1024]{};
    Read(object + offsets.FNameEntry.HeaderSize, wbuf, len * 2ull);
    return WideStringToUTF8(wbuf);
  }
  else {
    char buf[1024]{};
    Read(object + offsets.FNameEntry.HeaderSize, buf, len);
    if (Decrypt_ANSI) {
      Decrypt_ANSI(buf, len);
    }
    return std::string(buf);
  }
}


std::string UE_FNameEntry::WideStringToUTF8(const wchar_t* wideString)
{
  if (wideString == nullptr)
    return "";

  // 获取转换后的字符串长度（包括终止null字符）
  int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideString, -1, nullptr, 0, nullptr, nullptr);

  if (utf8Length == 0)
    return "";

  // 分配内存来保存转换后的UTF-8字符串
  char* utf8Buffer = new char[utf8Length];

  // 进行实际的转换
  WideCharToMultiByte(CP_UTF8, 0, wideString, -1, utf8Buffer, utf8Length, nullptr, nullptr);

  // 创建std::string并将转换后的UTF-8数据拷贝进去
  std::string utf8String(utf8Buffer);

  // 释放内存
  delete[] utf8Buffer;

  return utf8String;
}

void UE_FNameEntry::String(char* buf, bool wide, uint16 len) const {
  std::string tmp = String(wide, len);
  strcpy_s(buf, 1024, tmp.c_str());
}

std::string UE_FNameEntry::String() const {
  auto [wide, len] = this->Info();
  return this->String(wide, len);
}

uint16 UE_FNameEntry::Size(bool wide, uint16 len) {
  uint16 bytes = offsets.FNameEntry.HeaderSize + len * (wide ? 2 : 1);
  return (bytes + offsets.Stride - 1u) & ~(offsets.Stride - 1u);
}

std::string UE_FName::GetName() const {
  uint32 index = Read<uint32>(object);
  auto entry = UE_FNameEntry(NamePoolData.GetEntry(index));
  if (!entry) return std::string();
  auto [wide, len] = entry.Info();
  auto name = entry.String(wide, len);
  uint32 number = Read<uint32>(object + offsets.FName.Number);
  if (number > 0) {
    name += '_' + std::to_string(number);
  }
  auto pos = name.rfind('/');
  if (pos != std::string::npos) {
    name = name.substr(pos + 1);
  }
  return name;
}

uint32 UE_UObject::GetIndex() const {
  return Read<uint32>(object + offsets.UObject.Index);
};

UE_UClass UE_UObject::GetClass() const {
  return Read<UE_UClass>(object + offsets.UObject.Class);
}

UE_UObject UE_UObject::GetOuter() const {
  return Read<UE_UObject>(object + offsets.UObject.Outer);
}

UE_UObject UE_UObject::GetPackageObject() const {
  UE_UObject package(nullptr);
  for (auto outer = GetOuter(); outer; outer = outer.GetOuter()) {
    package = outer;
  }
  return package;
}

std::string UE_UObject::GetName() const {
  auto fname = UE_FName(object + offsets.UObject.Name);
  return fname.GetName();
}

std::string UE_UObject::GetFullName() const {
  std::string temp;
  for (auto outer = GetOuter(); outer; outer = outer.GetOuter()) {
    temp = outer.GetName() + "." + temp;
  }
  UE_UClass objectClass = GetClass();
  std::string name = objectClass.GetName() + " " + temp + GetName();
  return name;
}

std::string UE_UObject::GetCppName() const {
  std::string name;
  if (IsA<UE_UClass>()) {
    for (auto c = Cast<UE_UStruct>(); c; c = c.GetSuper()) {
      if (c == UE_AActor::StaticClass()) {
        name = "A";
        break;
      } else if (c == UE_UObject::StaticClass()) {
        name = "U";
        break;
      }
    }
  } else {
    name = "F";
  }

  name += GetName();
  return name;
}

bool UE_UObject::IsA(UE_UClass cmp) const {
  for (auto super = GetClass(); super; super = super.GetSuper().Cast<UE_UClass>()) {
    if (super == cmp) {
      return true;
    }
  }

  return false;
}

UE_UClass UE_UObject::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.Object"));
  return obj;
};

UE_UClass UE_AActor::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class Engine.Actor"));
  return obj;
}

UE_UField UE_UField::GetNext() const {
  return Read<UE_UField>(object + offsets.UField.Next);
}

UE_UClass UE_UField::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.Field"));
  return obj;
};

std::string IUProperty::GetName() const {
  return ((UE_UProperty*)(this->prop))->GetName();
}

int32 IUProperty::GetArrayDim() const {
  return ((UE_UProperty*)(this->prop))->GetArrayDim();
}

int32 IUProperty::GetSize() const {
  return ((UE_UProperty*)(this->prop))->GetSize();
}

int32 IUProperty::GetOffset() const {
  return ((UE_UProperty*)(this->prop))->GetOffset();
}

uint64 IUProperty::GetPropertyFlags() const {
  return ((UE_UProperty*)(this->prop))->GetPropertyFlags();
}

std::pair<PropertyType, std::string> IUProperty::GetType() const {
  return ((UE_UProperty*)(this->prop))->GetType();
}

uint8 IUProperty::GetFieldMask() const {
  return ((UE_UBoolProperty *)(this->prop))->GetFieldMask();
}

int32 UE_UProperty::GetArrayDim() const {
  return Read<int32>(object + offsets.UProperty.ArrayDim);
}

int32 UE_UProperty::GetSize() const {
  return Read<int32>(object + offsets.UProperty.ElementSize);
}

int32 UE_UProperty::GetOffset() const {
  return Read<int32>(object + offsets.UProperty.Offset);
}

uint64 UE_UProperty::GetPropertyFlags() const {
  return Read<uint64>(object + offsets.UProperty.PropertyFlags);
}

std::pair<PropertyType, std::string> UE_UProperty::GetType() const {
  if (IsA<UE_UDoubleProperty>()) { return {PropertyType::DoubleProperty,Cast<UE_UDoubleProperty>().GetTypeStr()}; };
  if (IsA<UE_UFloatProperty>()) { return {PropertyType::FloatProperty, Cast<UE_UFloatProperty>().GetTypeStr()}; };
  if (IsA<UE_UIntProperty>()) { return {PropertyType::IntProperty, Cast<UE_UIntProperty>().GetTypeStr()}; };
  if (IsA<UE_UInt16Property>()) { return {PropertyType::Int16Property,Cast<UE_UInt16Property>().GetTypeStr()}; };
  if (IsA<UE_UInt64Property>()) { return {PropertyType::Int64Property, Cast<UE_UInt64Property>().GetTypeStr()}; };
  if (IsA<UE_UInt8Property>()) { return {PropertyType::Int8Property, Cast<UE_UInt8Property>().GetTypeStr()}; };
  if (IsA<UE_UUInt16Property>()) { return {PropertyType::UInt16Property, Cast<UE_UUInt16Property>().GetTypeStr()}; };
  if (IsA<UE_UUInt32Property>()) { return {PropertyType::UInt32Property, Cast<UE_UUInt32Property>().GetTypeStr()}; }
  if (IsA<UE_UUInt64Property>()) { return {PropertyType::UInt64Property, Cast<UE_UUInt64Property>().GetTypeStr()}; };
  if (IsA<UE_UTextProperty>()) { return {PropertyType::TextProperty, Cast<UE_UTextProperty>().GetTypeStr()}; }
  if (IsA<UE_UStrProperty>()) { return {PropertyType::TextProperty, Cast<UE_UStrProperty>().GetTypeStr()}; };
  if (IsA<UE_UClassProperty>()) { return {PropertyType::ClassProperty, Cast<UE_UClassProperty>().GetTypeStr()}; };
  if (IsA<UE_UStructProperty>()) { return {PropertyType::StructProperty, Cast<UE_UStructProperty>().GetTypeStr()}; };
  if (IsA<UE_UNameProperty>()) { return {PropertyType::NameProperty, Cast<UE_UNameProperty>().GetTypeStr()}; };
  if (IsA<UE_UBoolProperty>()) { return {PropertyType::BoolProperty, Cast<UE_UBoolProperty>().GetTypeStr()}; }
  if (IsA<UE_UByteProperty>()) { return {PropertyType::ByteProperty, Cast<UE_UByteProperty>().GetTypeStr()}; };
  if (IsA<UE_UArrayProperty>()) { return {PropertyType::ArrayProperty, Cast<UE_UArrayProperty>().GetTypeStr()}; };
  if (IsA<UE_UEnumProperty>()) { return {PropertyType::EnumProperty, Cast<UE_UEnumProperty>().GetTypeStr()}; };
  if (IsA<UE_USetProperty>()) { return {PropertyType::SetProperty, Cast<UE_USetProperty>().GetTypeStr()}; };
  if (IsA<UE_UMapProperty>()) { return {PropertyType::MapProperty, Cast<UE_UMapProperty>().GetTypeStr()}; };
  if (IsA<UE_UInterfaceProperty>()) { return {PropertyType::InterfaceProperty, Cast<UE_UInterfaceProperty>().GetTypeStr()}; };
  if (IsA<UE_UMulticastDelegateProperty>()) { return {PropertyType::MulticastDelegateProperty, Cast<UE_UMulticastDelegateProperty>().GetTypeStr()}; };
  if (IsA<UE_UWeakObjectProperty>()) { return { PropertyType::WeakObjectProperty, Cast<UE_UWeakObjectProperty>().GetTypeStr() }; };
  if (IsA<UE_UObjectPropertyBase>()) { return {PropertyType::ObjectProperty, Cast<UE_UObjectPropertyBase>().GetTypeStr()}; };
  return {PropertyType::Unknown, GetClass().GetName()};
}

IUProperty UE_UProperty::GetInterface() const { return IUProperty(this); }

UE_UClass UE_UProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.Property"));
  return obj;
}

UE_UStruct UE_UStruct::GetSuper() const {
  return Read<UE_UStruct>(object + offsets.UStruct.SuperStruct);
}

UE_FField UE_UStruct::GetChildProperties() const {
  if (offsets.UStruct.ChildProperties) return Read<UE_FField>(object + offsets.UStruct.ChildProperties);
  else return nullptr;
}

UE_UField UE_UStruct::GetChildren() const {
  return Read<UE_UField>(object + offsets.UStruct.Children);
}

int32 UE_UStruct::GetSize() const {
  return Read<int32>(object + offsets.UStruct.PropertiesSize);
};

UE_UClass UE_UStruct::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.Struct"));
  return obj;
};

uint64 UE_UFunction::GetFunc() const {
  return Read<uint64>(object + offsets.UFunction.Func);
}

uint32 UE_UFunction::GetFunctionFlagInt() const {
  auto flags = Read<uint32>(object + offsets.UFunction.FunctionFlags);
  return flags;
}

void GetFlagOutVector(uint32 flag, std::vector<std::string>& out) {
  auto flags = flag;
  if (flags == FUNC_None) {
    out.push_back("None");
    return;
  }
  else {
    if (flags & FUNC_Final) {
      out.push_back("Final");
    }
    if (flags & FUNC_RequiredAPI) {
      out.push_back("RequiredAPI");
    }
    if (flags & FUNC_BlueprintAuthorityOnly) {
      out.push_back("BlueprintAuthorityOnly");
    }
    if (flags & FUNC_BlueprintCosmetic) {
      out.push_back("BlueprintCosmetic");
    }
    if (flags & FUNC_Net) {
      out.push_back("Net");
    }
    if (flags & FUNC_NetReliable) {
      out.push_back("NetReliable");
    }
    if (flags & FUNC_NetRequest) {
      out.push_back("NetRequest");
    }
    if (flags & FUNC_Exec) {
      out.push_back("Exec");
    }
    if (flags & FUNC_Native) {
      out.push_back("Native");
    }
    if (flags & FUNC_Event) {
      out.push_back("Event");
    }
    if (flags & FUNC_NetResponse) {
      out.push_back("NetResponse");
    }
    if (flags & FUNC_Static) {
      out.push_back("Static");
    }
    if (flags & FUNC_NetMulticast) {
      out.push_back("NetMulticast");
    }
    if (flags & FUNC_UbergraphFunction) {
      out.push_back("UbergraphFunction");
    }
    if (flags & FUNC_MulticastDelegate) {
      out.push_back("MulticastDelegate");
    }
    if (flags & FUNC_Public) {
      out.push_back("Public");
    }
    if (flags & FUNC_Private) {
      out.push_back("Private");
    }
    if (flags & FUNC_Protected) {
      out.push_back("Protected");
    }
    if (flags & FUNC_Delegate) {
      out.push_back("Delegate");
    }
    if (flags & FUNC_NetServer) {
      out.push_back("NetServer");
    }
    if (flags & FUNC_HasOutParms) {
      out.push_back("HasOutParms");
    }
    if (flags & FUNC_HasDefaults) {
      out.push_back("HasDefaults");
    }
    if (flags & FUNC_NetClient) {
      out.push_back("NetClient");
    }
    if (flags & FUNC_DLLImport) {
      out.push_back("DLLImport");
    }
    if (flags & FUNC_BlueprintCallable) {
      out.push_back("BlueprintCallable");
    }
    if (flags & FUNC_BlueprintEvent) {
      out.push_back("BlueprintEvent");
    }
    if (flags & FUNC_BlueprintPure) {
      out.push_back("BlueprintPure");
    }
    if (flags & FUNC_EditorOnly) {
      out.push_back("EditorOnly");
    }
    if (flags & FUNC_Const) {
      out.push_back("Const");
    }
    if (flags & FUNC_NetValidate) {
      out.push_back("NetValidate");
    }
  }
}

std::string UE_UFunction::GetFunctionFlags() const {
  auto flags = Read<uint32>(object + offsets.UFunction.FunctionFlags);
  std::string result;
  if (flags == FUNC_None) {
    result = "None";
  } else {
    if (flags & FUNC_Final) {
      result += "Final|";
    }
    if (flags & FUNC_RequiredAPI) {
      result += "RequiredAPI|";
    }
    if (flags & FUNC_BlueprintAuthorityOnly) {
      result += "BlueprintAuthorityOnly|";
    }
    if (flags & FUNC_BlueprintCosmetic) {
      result += "BlueprintCosmetic|";
    }
    if (flags & FUNC_Net) {
      result += "Net|";
    }
    if (flags & FUNC_NetReliable) {
      result += "NetReliable";
    }
    if (flags & FUNC_NetRequest) {
      result += "NetRequest|";
    }
    if (flags & FUNC_Exec) {
      result += "Exec|";
    }
    if (flags & FUNC_Native) {
      result += "Native|";
    }
    if (flags & FUNC_Event) {
      result += "Event|";
    }
    if (flags & FUNC_NetResponse) {
      result += "NetResponse|";
    }
    if (flags & FUNC_Static) {
      result += "Static|";
    }
    if (flags & FUNC_NetMulticast) {
      result += "NetMulticast|";
    }
    if (flags & FUNC_UbergraphFunction) {
      result += "UbergraphFunction|";
    }
    if (flags & FUNC_MulticastDelegate) {
      result += "MulticastDelegate|";
    }
    if (flags & FUNC_Public) {
      result += "Public|";
    }
    if (flags & FUNC_Private) {
      result += "Private|";
    }
    if (flags & FUNC_Protected) {
      result += "Protected|";
    }
    if (flags & FUNC_Delegate) {
      result += "Delegate|";
    }
    if (flags & FUNC_NetServer) {
      result += "NetServer|";
    }
    if (flags & FUNC_HasOutParms) {
      result += "HasOutParms|";
    }
    if (flags & FUNC_HasDefaults) {
      result += "HasDefaults|";
    }
    if (flags & FUNC_NetClient) {
      result += "NetClient|";
    }
    if (flags & FUNC_DLLImport) {
      result += "DLLImport|";
    }
    if (flags & FUNC_BlueprintCallable) {
      result += "BlueprintCallable|";
    }
    if (flags & FUNC_BlueprintEvent) {
      result += "BlueprintEvent|";
    }
    if (flags & FUNC_BlueprintPure) {
      result += "BlueprintPure|";
    }
    if (flags & FUNC_EditorOnly) {
      result += "EditorOnly|";
    }
    if (flags & FUNC_Const) {
      result += "Const|";
    }
    if (flags & FUNC_NetValidate) {
      result += "NetValidate|";
    }
    if (result.size()) {
      result.erase(result.size() - 1);
    }
  }
  return result;
}

UE_UClass UE_UFunction::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.Function"));
  return obj;
}

UE_UClass UE_UScriptStruct::StaticClass() {
  static UE_UClass obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.ScriptStruct"));
  return obj;
};

UE_UClass UE_UClass::StaticClass() {
  static UE_UClass obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.Class"));
  return obj;
};

TArray UE_UEnum::GetNames() const {
  return Read<TArray>(object + offsets.UEnum.Names);
}

UE_UClass UE_UEnum::StaticClass() {
  static UE_UClass obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.Enum"));
  return obj;
}

std::string UE_UDoubleProperty::GetTypeStr() const { return "double"; }

UE_UClass UE_UDoubleProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.DoubleProperty"));
  return obj;
}

UE_UStruct UE_UStructProperty::GetStruct() const {
  return Read<UE_UStruct>(object + offsets.UProperty.Size);
}

std::string UE_UStructProperty::GetTypeStr() const {
  return "struct " + GetStruct().GetCppName();
}

UE_UClass UE_UStructProperty::StaticClass() {
  static auto obj = (UE_UClass)( ObjObjects.FindObject("Class CoreUObject.StructProperty"));
  return obj;
}

std::string UE_UNameProperty::GetTypeStr() const { return "struct FName"; }

UE_UClass UE_UNameProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.NameProperty"));
  return obj;
}

UE_UClass UE_UObjectPropertyBase::GetPropertyClass() const {
  return Read<UE_UClass>(object + offsets.UProperty.Size);
}

std::string UE_UObjectPropertyBase::GetTypeStr() const {
  return "struct " + GetPropertyClass().GetCppName() + "*";
}

UE_UClass UE_UObjectPropertyBase::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.ObjectPropertyBase"));
  return obj;
}

UE_UProperty UE_UArrayProperty::GetInner() const {
  return Read<UE_UProperty>(object + offsets.UProperty.Size);
}

std::string UE_UArrayProperty::GetTypeStr() const {
  return "struct TArray<" + GetInner().GetType().second + ">";
}

UE_UClass UE_UArrayProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.ArrayProperty"));
  return obj;
}

UE_UEnum UE_UByteProperty::GetEnum() const {
  return Read<UE_UEnum>(object + offsets.UProperty.Size);
}

std::string UE_UByteProperty::GetTypeStr() const {
  auto e = GetEnum();
  if (e) return "enum class " + e.GetName();
  return "char";
}

UE_UClass UE_UByteProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.ByteProperty"));
  return obj;
}

uint8 UE_UBoolProperty::GetFieldMask() const {
  return Read<uint8>(object + offsets.UProperty.Size + 3);
}

std::string UE_UBoolProperty::GetTypeStr() const {
  if (GetFieldMask() == 0xFF) {
    return "bool";
  };
  return "char";
}

UE_UClass UE_UBoolProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.BoolProperty"));
  return obj;
}

std::string UE_UFloatProperty::GetTypeStr() const { return "float"; }

UE_UClass UE_UFloatProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.FloatProperty"));
  return obj;
}

std::string UE_UIntProperty::GetTypeStr() const { return "int"; }

UE_UClass UE_UIntProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.IntProperty"));
  return obj;
}

std::string UE_UInt16Property::GetTypeStr() const { return "int16"; }

UE_UClass UE_UInt16Property::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.Int16Property"));
  return obj;
}

std::string UE_UInt64Property::GetTypeStr() const { return "int64"; }

UE_UClass UE_UInt64Property::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.Int64Property"));
  return obj;
}

std::string UE_UInt8Property::GetTypeStr() const { return "uint8"; }

UE_UClass UE_UInt8Property::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.Int8Property"));
  return obj;
}

std::string UE_UUInt16Property::GetTypeStr() const { return "uint16"; }

UE_UClass UE_UUInt16Property::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.UInt16Property"));
  return obj;
}

std::string UE_UUInt32Property::GetTypeStr() const { return "uint32"; }

UE_UClass UE_UUInt32Property::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.UInt32Property"));
  return obj;
}

std::string UE_UUInt64Property::GetTypeStr() const { return "uint64"; }

UE_UClass UE_UUInt64Property::StaticClass() {
  static auto obj = (UE_UClass)( ObjObjects.FindObject("Class CoreUObject.UInt64Property"));
  return obj;
}

std::string UE_UTextProperty::GetTypeStr() const { return "struct FText"; }

UE_UClass UE_UTextProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.TextProperty"));
  return obj;
}

std::string UE_UStrProperty::GetTypeStr() const { return "struct FString"; }

UE_UClass UE_UStrProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.StrProperty"));
  return obj;
}

UE_UClass UE_UEnumProperty::GetEnum() const {
  return Read<UE_UClass>(object + offsets.UProperty.Size + 8);
}

std::string UE_UEnumProperty::GetTypeStr() const {
  return "enum class " + GetEnum().GetName();
}

UE_UClass UE_UEnumProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.EnumProperty"));
  return obj;
}

UE_UClass UE_UClassProperty::GetMetaClass() const {
  return Read<UE_UClass>(object + offsets.UProperty.Size + 8);
}

std::string UE_UClassProperty::GetTypeStr() const {
  return "struct " + GetMetaClass().GetCppName() + "*";
}

UE_UClass UE_UClassProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.ClassProperty"));
  return obj;
}

UE_UProperty UE_USetProperty::GetElementProp() const {
  return Read<UE_UProperty>(object + offsets.UProperty.Size);
}

std::string UE_USetProperty::GetTypeStr() const {
  return "struct TSet<" + GetElementProp().GetType().second + ">";
}

UE_UClass UE_USetProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.SetProperty"));
  return obj;
}

UE_UProperty UE_UMapProperty::GetKeyProp() const {
  return Read<UE_UProperty>(object + offsets.UProperty.Size);
}

UE_UProperty UE_UMapProperty::GetValueProp() const {
  return Read<UE_UProperty>(object + offsets.UProperty.Size + 8);
}

std::string UE_UMapProperty::GetTypeStr() const {
  return fmt::format("struct TMap<{}, {}>", GetKeyProp().GetType().second, GetValueProp().GetType().second);
}

UE_UClass UE_UMapProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.MapProperty"));
  return obj;
}

UE_UProperty UE_UInterfaceProperty::GetInterfaceClass() const {
  return Read<UE_UProperty>(object + offsets.UProperty.Size);
}

std::string UE_UInterfaceProperty::GetTypeStr() const {
  return "struct TScriptInterface<" + GetInterfaceClass().GetType().second + ">";
}

UE_UClass UE_UInterfaceProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.InterfaceProperty"));
  return obj;
}

std::string UE_UMulticastDelegateProperty::GetTypeStr() const {
  return "struct FScriptMulticastDelegate";
}

UE_UClass UE_UMulticastDelegateProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.MulticastDelegateProperty"));
  return obj;
}

std::string UE_UWeakObjectProperty::GetTypeStr() const {
  return "struct TWeakObjectPtr<" + this->Cast<UE_UStructProperty>().GetTypeStr() + ">";
}

UE_UClass UE_UWeakObjectProperty::StaticClass() {
  static auto obj = (UE_UClass)(ObjObjects.FindObject("Class CoreUObject.WeakObjectProperty"));
  return obj;
}

std::string UE_FFieldClass::GetName() const {
  auto name = UE_FName(object);
  return name.GetName();
}

UE_FField UE_FField::GetNext() const {
  return Read<UE_FField>(object + offsets.FField.Next);
};

std::string UE_FField::GetName() const {
  auto name = UE_FName(object + offsets.FField.Name);
  return name.GetName();
}

std::string IFProperty::GetName() const {
  return ((UE_FProperty *)prop)->GetName();
}

int32 IFProperty::GetArrayDim() const {
  return ((UE_FProperty*)prop)->GetArrayDim();
}

int32 IFProperty::GetSize() const { return ((UE_FProperty *)prop)->GetSize(); }

int32 IFProperty::GetOffset() const {
  return ((UE_FProperty*)prop)->GetOffset();
}

uint64 IFProperty::GetPropertyFlags() const {
  return ((UE_FProperty*)prop)->GetPropertyFlags();
}

std::pair<PropertyType, std::string> IFProperty::GetType() const {
  return ((UE_FProperty*)prop)->GetType();
}

uint8 IFProperty::GetFieldMask() const {
  return ((UE_FBoolProperty *)prop)->GetFieldMask();
}

int32 UE_FProperty::GetArrayDim() const {
  return Read<int32>(object + offsets.FProperty.ArrayDim);
}

int32 UE_FProperty::GetSize() const {
  return Read<int32>(object + offsets.FProperty.ElementSize);
}

int32 UE_FProperty::GetOffset() const {
  return Read<int32>(object + offsets.FProperty.Offset);
}

uint64 UE_FProperty::GetPropertyFlags() const {
  return Read<uint64>(object + offsets.FProperty.PropertyFlags);
}

type UE_FProperty::GetType() const {
  auto objectClass = Read<UE_FFieldClass>(object + offsets.FField.Class);
  type type = {PropertyType::Unknown, objectClass.GetName()};

  auto& str = type.second;
  auto hash = Hash(str.c_str(), str.size());
  switch (hash) {
  case HASH("StructProperty"): {
    auto obj = this->Cast<UE_FStructProperty>();
    type = { PropertyType::StructProperty, obj.GetTypeStr() };
    break;
  }
  case HASH("ObjectProperty"): {
    auto obj = this->Cast<UE_FObjectPropertyBase>();
    type = { PropertyType::ObjectProperty, obj.GetTypeStr() };
    break;
  }
  case HASH("SoftObjectProperty"): {
    auto obj = this->Cast<UE_FObjectPropertyBase>();
    type = { PropertyType::SoftObjectProperty, "struct TSoftObjectPtr<" + obj.GetPropertyClass().GetCppName() + ">" };
    break;
  }
  case HASH("FloatProperty"): {
    type = { PropertyType::FloatProperty, "float" };
    break;
  }
  case HASH("ByteProperty"): {
    auto obj = this->Cast<UE_FByteProperty>();
    type = { PropertyType::ByteProperty, obj.GetTypeStr() };
    break;
  }
  case HASH("BoolProperty"): {
    auto obj = this->Cast<UE_FBoolProperty>();
    type = { PropertyType::BoolProperty, obj.GetTypeStr() };
    break;
  }
  case HASH("IntProperty"): {
    type = { PropertyType::IntProperty, "int32_t" };
    break;
  }
  case HASH("Int8Property"): {
    type = { PropertyType::Int8Property, "int8_t" };
    break;
  }
  case HASH("Int16Property"): {
    type = { PropertyType::Int16Property, "int16_t" };
    break;
  }
  case HASH("Int64Property"): {
    type = { PropertyType::Int64Property, "int64_t" };
    break;
  }
  case HASH("UInt16Property"): {
    type = { PropertyType::UInt16Property, "uint16_t" };
    break;
  }
  case HASH("UInt32Property"): {
    type = { PropertyType::UInt32Property, "uint32_t" };
    break;
  }
  case HASH("UInt64Property"): {
    type = { PropertyType::UInt64Property, "uint64_t" };
    break;
  }
  case HASH("NameProperty"): {
    type = { PropertyType::NameProperty, "struct FName" };
    break;
  }
  case HASH("DelegateProperty"): {
    type = { PropertyType::DelegateProperty, "struct FDelegate" };
    break;
  }
  case HASH("SetProperty"): {
    auto obj = this->Cast<UE_FSetProperty>();
    type = { PropertyType::SetProperty, obj.GetTypeStr() };
    break;
  }
  case HASH("ArrayProperty"): {
    auto obj = this->Cast<UE_FArrayProperty>();
    type = { PropertyType::ArrayProperty, obj.GetTypeStr() };
    break;
  }
  case HASH("WeakObjectProperty"): {
    auto obj = this->Cast<UE_FStructProperty>();
    type = { PropertyType::WeakObjectProperty, "struct TWeakObjectPtr<" + obj.GetTypeStr() + ">" };

    break;
  }
  case HASH("StrProperty"): {
    type = { PropertyType::StrProperty, "struct FString" };
    break;
  }
  case HASH("TextProperty"): {
    type = { PropertyType::TextProperty, "struct FText" };
    break;
  }
  case HASH("MulticastSparseDelegateProperty"): {
    type = { PropertyType::MulticastSparseDelegateProperty, "struct FMulticastSparseDelegate" };
    break;
  }
  case HASH("EnumProperty"): {
    auto obj = this->Cast<UE_FEnumProperty>();
    type = { PropertyType::EnumProperty, obj.GetTypeStr() };
    break;
  }
  case HASH("DoubleProperty"): {
    type = { PropertyType::DoubleProperty, "double" };
    break;
  }
  case HASH("MulticastDelegateProperty"): {
    type = { PropertyType::MulticastDelegateProperty, "FMulticastDelegate" };
    break;
  }
  case HASH("ClassProperty"): {
    auto obj = this->Cast<UE_FClassProperty>();
    type = { PropertyType::ClassProperty, obj.GetTypeStr() };
    break;
  }
  case HASH("MulticastInlineDelegateProperty"): {
    type = { PropertyType::MulticastDelegateProperty, "struct FMulticastInlineDelegate" };
    break;
  }
  case HASH("MapProperty"): {
    auto obj = this->Cast<UE_FMapProperty>();
    type = { PropertyType::MapProperty, obj.GetTypeStr() };
    break;
  }
  case HASH("InterfaceProperty"): {
    auto obj = this->Cast<UE_FInterfaceProperty>();
    type = { PropertyType::InterfaceProperty, obj.GetTypeStr() };
    break;
  }
  case HASH("FieldPathProperty"): {
    auto obj = this->Cast<UE_FFieldPathProperty>();
    type = { PropertyType::FieldPathProperty, obj.GetTypeStr() };
    break;
  }
  case HASH("SoftClassProperty"): {
    type = { PropertyType::SoftClassProperty, "struct TSoftClassPtr<UObject>" };
    break;
  }
  }

  return type;
}

IFProperty UE_FProperty::GetInterface() const { return IFProperty(this); }

UE_UStruct UE_FStructProperty::GetStruct() const {
  return Read<UE_UStruct>(object + offsets.FProperty.Size);
}

std::string UE_FStructProperty::GetTypeStr() const {
  return "struct " + GetStruct().GetCppName();
}

UE_UClass UE_FObjectPropertyBase::GetPropertyClass() const {
  return Read<UE_UClass>(object + offsets.FProperty.Size);
}

std::string UE_FObjectPropertyBase::GetTypeStr() const {
  return "struct " + GetPropertyClass().GetCppName() + "*";
}

UE_FProperty UE_FArrayProperty::GetInner() const {
  return Read<UE_FProperty>(object + offsets.FProperty.Size);
}

std::string UE_FArrayProperty::GetTypeStr() const {
  return "struct TArray<" + GetInner().GetType().second + ">";
}

UE_UEnum UE_FByteProperty::GetEnum() const {
  return Read<UE_UEnum>(object + offsets.FProperty.Size);
}

std::string UE_FByteProperty::GetTypeStr() const {
  auto e = GetEnum();
  if (e) return "enum class " + e.GetName();
  return "char";
}

uint8 UE_FBoolProperty::GetFieldMask() const {
  return Read<uint8>(object + offsets.FProperty.Size + 3);
}

std::string UE_FBoolProperty::GetTypeStr() const {
  if (GetFieldMask() == 0xFF) {
    return "bool";
  };
  return "char";
}

UE_UClass UE_FEnumProperty::GetEnum() const {
  return Read<UE_UClass>(object + offsets.FProperty.Size + 8);
}

std::string UE_FEnumProperty::GetTypeStr() const {
  return "enum class " + GetEnum().GetName();
}

UE_UClass UE_FClassProperty::GetMetaClass() const {
  return Read<UE_UClass>(object + offsets.FProperty.Size);
}

std::string UE_FClassProperty::GetTypeStr() const {
  return "struct " + GetMetaClass().GetCppName() + "*";
}

UE_FProperty UE_FSetProperty::GetElementProp() const {
  return Read<UE_FProperty>(object + offsets.FProperty.Size);
}

std::string UE_FSetProperty::GetTypeStr() const {
  return "struct TSet<" + GetElementProp().GetType().second + ">";
}

UE_FProperty UE_FMapProperty::GetKeyProp() const {
  return Read<UE_FProperty>(object + offsets.FProperty.Size);
}

UE_FProperty UE_FMapProperty::GetValueProp() const {
  return Read<UE_FProperty>(object + offsets.FProperty.Size + 8);
}

std::string UE_FMapProperty::GetTypeStr() const {
  return fmt::format("struct TMap<{}, {}>", GetKeyProp().GetType().second, GetValueProp().GetType().second);
}

UE_UClass UE_FInterfaceProperty::GetInterfaceClass() const {
  return Read<UE_UClass>(object + offsets.FProperty.Size);
}

std::string UE_FInterfaceProperty::GetTypeStr() const {
  return "struct TScriptInterface<I" + GetInterfaceClass().GetName() + ">";
}

UE_FName UE_FFieldPathProperty::GetPropertyName() const {
  return Read<UE_FName>(object + offsets.FProperty.Size);
}

std::string UE_FFieldPathProperty::GetTypeStr() const {
  return "struct TFieldPath<F" + GetPropertyName().GetName() + ">";
}

void UE_UPackage::GenerateBitPadding(std::vector<Member>& members, uint32 offset, uint8 bitOffset, uint8 size) {
  Member padding;
  padding.Type = "char";
  padding.Name = fmt::format("pad_{:0X}_{} : {}", offset, bitOffset, size);
  padding.Offset = offset;
  padding.Size = 1;
  members.push_back(padding);
}

void UE_UPackage::GeneratePadding(std::vector<Member>& members, uint32 offset, uint32 size) {
  Member padding;
  padding.Type = "char";
  padding.Name = fmt::format("pad_{:0X}[{:#0x}]", offset, size);
  padding.Offset = offset;
  padding.Size = size;
  members.push_back(padding);
}

void UE_UPackage::FillPadding(UE_UStruct object, std::vector<Member>& members, uint32& offset, uint8& bitOffset, uint32 end, bool findPointers) {
  if (bitOffset && bitOffset < 8) {
    UE_UPackage::GenerateBitPadding(members, offset, bitOffset, 8 - bitOffset);
    bitOffset = 0;
    offset++;
  }

  auto size = end - offset;
  if (findPointers && size >= 8) {

    auto normalizedOffset = (offset + 7) & ~7;

    if (normalizedOffset != offset) {
      auto diff = normalizedOffset - offset;
      GeneratePadding(members, offset, diff);
      offset += diff;
    }

    auto normalizedSize = size - size % 8;

    auto num = normalizedSize / 8;

    uint64* pointers = new uint64[num * 2]();
    uint64* buffer = pointers + num;

    uint32 found = 0;
    auto callback = [&](UE_UObject object) {

      auto address = (uint64*)((uint64)object.GetAddress() + offset);

      Read(address, buffer, normalizedSize);

      for (uint32 i = 0; i < num; i++) {

        if (pointers[i]) continue;

        auto ptr = buffer[i];
        if (!ptr) continue;

        uint64 vftable;
        if (Read((void*)ptr, &vftable, 8)) {
          pointers[i] = ptr;
        }
        else {
          pointers[i] = (uint64)-1;
        }

        found++;
      }

      if (found == num) return true;

      return false;

    };

    ObjObjects.ForEachObjectOfClass((UE_UClass)object, callback);

    auto start = offset;
    for (uint32 i = 0; i < num; i++) {
      auto ptr = pointers[i];
      if (ptr && ptr != (uint64)-1) {

        auto ptrObject = UE_UObject((void*)ptr);

        auto ptrOffset = start + i * 8;
        if (ptrOffset > offset) {
          GeneratePadding(members, offset, ptrOffset - offset);
          offset = ptrOffset;
        }

        Member m;
        m.Offset = offset;
        m.Size = 8;

        if (ptrObject.IsA<UE_UObject>()) {
          m.Type = "struct " + ptrObject.GetClass().GetCppName() + "*";
          m.Name = ptrObject.GetName();
        }
        else {
          m.Type = "void*";
          m.Name = fmt::format("ptr_{:x}", ptr);
        }


        members.push_back(m);

        offset += 8;
      }
    }
    delete[] pointers;

  }


  if (offset != end) {
    GeneratePadding(members, offset, end - offset);
    offset = end;
  }
}

void UE_UPackage::GenerateFunction(UE_UFunction fn, Function *out, std::unordered_map<std::string, int>& memberMap) {
  out->FullName = ProcessUTF8Char(fn.GetFullName());
  out->Flags = fn.GetFunctionFlags();
  out->FuncFlag = fn.GetFunctionFlagInt();
  out->Func = fn.GetFunc();
  out->FuncName = fn.GetName();
  out->FuncName = GetValidClassName(ProcessUTF8Char(out->FuncName));
  if(memberMap.count(out->FuncName) > 0) {
    out->FuncName += fmt::format("_{}", ++memberMap[out->FuncName]);
  }
  if (out->FuncFlag & FUNC_Static) {
    out->FuncName = "STATIC_" + out->FuncName;
  }

  std::unordered_map<std::string, int> paramCntMp;
  auto generateParam = [&](IProperty *prop) {
    
    auto param_offset = prop->GetOffset();
    auto param_size = prop->GetSize();
    auto flags = prop->GetPropertyFlags();
    // if property has 'ReturnParm' flag
    if (flags & 0x400) {
      ParamInfo retInfo;
      out->RetType = prop->GetType().second;
      retInfo.Name = "ReturnValue";
      retInfo.Offset = param_offset;
      retInfo.Size = param_size;
      retInfo.Type = out->RetType;
      retInfo.flags = flags;
      out->paramInfo.push_back(retInfo);
      out->CppName = prop->GetType().second + " " + out->FuncName;
    }
    // if property has 'Parm' flag
    else if (flags & 0x80) {
      ParamInfo paramInfo;
      out->ParamTypes.push_back(prop->GetType().second);
      auto ParamName = GetValidClassName(prop->GetName());
      if (paramCntMp.count(ParamName) > 0) {
        ParamName += fmt::format("_{}", ++paramCntMp[ParamName]);
      }
      else {
        paramCntMp[ParamName] = 1;
      }
      if (ParamName[0] >= '0' && ParamName[0] <= '9') {
        ParamName = "_" + ParamName;
      }
      paramInfo.Offset = param_offset;
      paramInfo.Size = param_size;
      paramInfo.flags = flags;
      paramInfo.Name = ParamName;
      if (prop->GetArrayDim() > 1) {
        out->Params += fmt::format("{}* {}, ", prop->GetType().second, ParamName);
        paramInfo.Type = fmt::format("{}*", prop->GetType().second);
      } else {
        if (flags & 0x100) {
          out->Params += fmt::format("{}& {}, ", prop->GetType().second, ParamName);
          paramInfo.Type = fmt::format("{}&", prop->GetType().second);
        }
        else {
          out->Params += fmt::format("{} {}, ", prop->GetType().second, ParamName);
          paramInfo.Type = prop->GetType().second;
        }
      }
      out->paramInfo.push_back(paramInfo);
    }
  };

  for (auto prop = fn.GetChildProperties().Cast<UE_FProperty>(); prop; prop = prop.GetNext().Cast<UE_FProperty>()) {
    auto propInterface = prop.GetInterface();
    generateParam(&propInterface);
  }
  for (auto prop = fn.GetChildren().Cast<UE_UProperty>(); prop; prop = prop.GetNext().Cast<UE_UProperty>()) {
    auto propInterface = prop.GetInterface();
    generateParam(&propInterface);
  }
  if (out->Params.size()) {
    out->Params.erase(out->Params.size() - 2);
  }

  if (out->CppName.size() == 0) {
    out->RetType = "void";
    out->CppName = "void " + out->FuncName;
  }
}

std::string to_hex_string(int value) {
  std::stringstream stream;
  stream << std::setfill('0') << std::setw(2) << std::hex << value;
  return stream.str();
}

std::string UE_UPackage::ProcessUTF8Char(std::string input) {
  // 移除空字符
  std::string tmp;
  for (char ch : input) {
    if (ch != '\0') {
      tmp += ch;
    }
  }
  std::string result;
  // 转义非法字符
  for (std::size_t i = 0; i < tmp.size(); ++i) {
    char ch = tmp[i];
    if ((ch & 0x80) == 0x00) {
      // Single-byte character (ASCII range)
      result += ch;
    }
    else if ((ch & 0xE0) == 0xC0) {
      // Two-byte character (UTF-8 encoded Chinese character)
      result += ch;
      result += tmp[++i];
    }
    else if ((ch & 0xF0) == 0xE0) {
      // Three-byte character (UTF-8 encoded Chinese character)
      result += ch;
      result += tmp[++i];
      result += tmp[++i];
    }
    else {
      // Not a valid UTF-8 character, replace with byte code
      result += "_x" + to_hex_string(static_cast<unsigned char>(ch));
    }
  }
  return result;
}

std::string UE_UPackage::GetCpp_xString(std::string& input) {
  std::string result = "";
  for (char& c : input) {
    result += "\\x" + to_hex_string(static_cast<unsigned char>(c));
  }
  return result;
}

std::string UE_UPackage::GetValidClassName(std::string str) {
  if(str[0] >= '0' && str[0] <= '9') {
    str = "_" + str;
  }
  // step1: 替换非法的字符
  char chars[] = " /\\:*?\"<>|+().&-=![]{}\'";
  for (auto c : chars) {
    size_t pos = str.find(c);
    while (pos != std::string::npos) {
      str[pos] = '_';
      pos = str.find(c);
    }
  }
  str = ProcessUTF8Char(str);

  return str;
}

// To solve the redefine of the same class name
std::unordered_map<std::string, int> typeDefCnt;

void UE_UPackage::GenerateStruct(UE_UStruct object, std::vector<Struct>& arr, bool findPointers) {
  Struct s;
  //s.Size = object.GetSize();
  if(ClassSizeFixer::sizeMp.count(object.GetAddress()))
    s.Size = ClassSizeFixer::sizeMp[object.GetAddress()]; // Fix the class
  else
    s.Size = object.GetSize();
  if (s.Size == 0) {
    // return;
  }
  s.Inherited = 0;

  s.FullName = ProcessUTF8Char(object.GetFullName());
  s.ClassName = object.GetCppName();

  if (s.ClassName == "UWorld") {
    // 插入Gworld 惊静态成员变量
    Member static_gworld;
    static_gworld.Type = "static class UWorld**";
    static_gworld.Offset = 0;
    static_gworld.Name = "GWorld";
    static_gworld.Size = 8;
    s.Members.push_back(static_gworld);
  }

  if (typeDefCnt.count(s.ClassName)) {
    s.ClassName += fmt::format("_def{}", ++typeDefCnt[s.ClassName]);
  }
  else {
    typeDefCnt[s.ClassName] = 1;
  }

  if (object.IsA<UE_UClass>()) {
    s.CppName = "class " + GetValidClassName(s.ClassName);
  }
  else {
    s.CppName = "struct " + GetValidClassName(s.ClassName);
  }
  
  s.SuperName = GetValidClassName(object.GetSuper().GetCppName());

  auto super = object.GetSuper();
  
  if (super) {
    s.CppName += " : public " + s.SuperName;
    //s.Inherited = super.GetSize();
    s.Inherited = ClassSizeFixer::sizeMp[super.GetAddress()];
  }

  uint32 offset = s.Inherited;

  uint8 bitOffset = 0;
  std::unordered_map<std::string, int> memberNameCntMp;
  std::unordered_map<std::string, int> functionNameCntMp;

  auto generateMember = [&](IProperty *prop, Member *m) {
    auto arrDim = prop->GetArrayDim();
    m->Size = prop->GetSize() * arrDim;
    m->isSuspectMember = false;
    if (m->Size == 0) {
      return;
    } // this shouldn't be zero

    auto type = prop->GetType();
    m->Type = type.second;
    
    m->Name = prop->GetName();

    FixKeywordConflict(m->Name);
    m->Name = GetValidClassName(m->Name);
    m->Offset = prop->GetOffset();

    if (m->Name[0] >= '0' && m->Name[0] <= '9') {
      m->Name = "_" + m->Name;
    }

    if (memberNameCntMp.count(m->Name) > 0) {
      memberNameCntMp[m->Name]++;
      m->Name += fmt::format("_{}", memberNameCntMp[m->Name]);
    }
    else {
      memberNameCntMp[m->Name] = 1;
    }
    if (m->Offset < s.Inherited) {
      // Should be solved by ClassSizeFixer and this will not appear!
      // Impossible situation, but some game still fucking appear
      // mark the member to suspect member, and do not actually use it.
      printf("[Warning] Bad member offset: [%s]->[%s] offset: %X \n", s.FullName.c_str(), m->Name.c_str(), m->Offset);
      m->isSuspectMember = true;
      return;
    }
    if (m->Offset > offset) {
      UE_UPackage::FillPadding(object, s.Members, offset, bitOffset, m->Offset, findPointers);
    }
    if (type.first == PropertyType::BoolProperty && *(uint32*)type.second.data() != 'loob') {
      auto boolProp = prop;
      auto mask = boolProp->GetFieldMask();
      uint8 zeros = 0, ones = 0;
      while (mask & ~1) {
        mask >>= 1;
        zeros++;
      }
      while (mask & 1) {
        mask >>= 1;
        ones++;
      }
      if (zeros > bitOffset) {
        UE_UPackage::GenerateBitPadding(s.Members, offset, bitOffset, zeros - bitOffset);
        bitOffset = zeros;
      }
      m->Name += fmt::format(" : {}", ones);
      bitOffset += ones;

      if (bitOffset == 8) {
        offset++;
        bitOffset = 0;
      }

    } else {
      if (arrDim > 1) {
        m->Name += fmt::format("[{:#0x}]", arrDim);
      }

      offset += m->Size;
    }
  };

  for (auto prop = object.GetChildProperties().Cast<UE_FProperty>(); prop; prop = prop.GetNext().Cast<UE_FProperty>()) {
    Member m;
    auto propInterface = prop.GetInterface();
    generateMember(&propInterface, &m);
    s.Members.push_back(m);
  }

  for (auto child = object.GetChildren(); child; child = child.GetNext()) {
    if (child.IsA<UE_UProperty>()) {
      auto prop = child.Cast<UE_UProperty>();
      Member m;
      auto propInterface = prop.GetInterface();
      generateMember(&propInterface, &m);
      s.Members.push_back(m);
    }
  }

  for (auto child = object.GetChildren(); child; child = child.GetNext()) {
    if (child.IsA<UE_UFunction>()) {
      auto fn = child.Cast<UE_UFunction>();
      Function f;
      GenerateFunction(fn, &f, memberNameCntMp);
      // to avoid the repeat function name ...
      if (functionNameCntMp.count(f.FullName) == 0) {
        functionNameCntMp[f.FullName] = 1;
        s.Functions.push_back(f);
      }
    }
  }
  // 如果是USkeletalMeshComponent，则注入GetBoneWorldPos声明
  if (s.ClassName == "USkeletalMeshComponent") {
    Function GetBoneWorldPos_fn;
    GetBoneWorldPos_fn.CppName = "FVector GetBoneWorldPos";
    GetBoneWorldPos_fn.FuncName = "GetBoneWorldPos";
    GetBoneWorldPos_fn.Params = "const int32_t& boneId";
    GetBoneWorldPos_fn.RetType = "FVector";
    GetBoneWorldPos_fn.FullName = "Dumper_Generated_Function";
    GetBoneWorldPos_fn.Func = Base;
    GetBoneWorldPos_fn.declareConst = " const";
    s.Functions.push_back(GetBoneWorldPos_fn);
  }
  // 生成StaticClass方法
  {
    Function static_class_fn;
    static_class_fn.CppName = "static UClass* StaticClass";
    static_class_fn.FuncName = "StaticClass";
    static_class_fn.RetType = "UClass*";
    static_class_fn.FullName = "Dumper_Generated_Function";
    static_class_fn.Func = Base;
    s.Functions.push_back(static_class_fn);
  }

  if (s.Size > offset) {
    UE_UPackage::FillPadding(object, s.Members, offset, bitOffset, s.Size, findPointers);
  }

  arr.push_back(s);
}

void UE_UPackage::FixKeywordConflict(std::string& tocheck) {
  const std::vector<std::string> cppKeyword = {
    "alignas",
    "alignof",
    "and",
    "and_eq",
    "asm",
    "auto",
    "bitand",
    "bitor",
    "bool",
    "break",
    "case",
    "catch",
    "char",
    "char16_t",
    "char32_t",
    "class",
    "compl",
    "concept",
    "const",
    "constexpr",
    "const_cast",
    "continue",
    "decltype",
    "default",
    "delete",
    "do",
    "double",
    "dynamic_cast",
    "else",
    "enum",
    "explicit",
    "export",
    "extern",
    "false",
    "float",
    "for",
    "friend",
    "goto",
    "if",
    "inline",
    "int",
    "long",
    "mutable",
    "namespace",
    "new",
    "noexcept",
    "not",
    "not_eq",
    "nullptr",
    "operator",
    "or",
    "or_eq",
    "private",
    "protected",
    "public",
    "register",
    "reinterpret_cast",
    "requires",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "static_assert",
    "static_cast",
    "struct",
    "switch",
    "template",
    "this",
    "thread_local",
    "throw",
    "true",
    "try",
    "typedef",
    "typeid",
    "typename",
    "union",
    "unsigned",
    "using",
    "virtual",
    "void",
    "volatile",
    "wchar_t",
    "while",
    "xor",
    "xor_eq"
  };
  const std::vector<std::string> includeKeyword = {
    "IGNORE",
    "ABSOLUTE",
    "RELATIVE",
    "DEBUG",
    "RELEASE"
  };
  for (auto& keyword : cppKeyword) {
    if (tocheck == keyword) {
      tocheck += "_1";
      return;
    }
  }
  for (auto& keyword : includeKeyword) {
    if (tocheck == keyword) {
      tocheck += "_1";
      return;
    }
  }
}

void UE_UPackage::GenerateEnum(UE_UEnum object, std::vector<Enum> &arr) {
  Enum e;
  e.FullName = ProcessUTF8Char(object.GetFullName());
 
  auto names = object.GetNames();
  
  uint64 max = 0;
  uint64 nameSize = ((offsets.FName.Number + 4) + 7) & ~(7);
  uint64 pairSize = nameSize + 8;

  for (uint32 i = 0; i < names.Count; i++) {

    auto pair = names.Data + i * pairSize;
    auto name = UE_FName(pair);
    auto str = name.GetName();
    auto pos = str.find_last_of(':');
    if (pos != std::string::npos) {
      str = str.substr(pos + 1);
    }

    //auto value = Read<int64>(pair + nameSize);
    // this read some wrong value, so I force it to be ordered... May be wrong?
    auto value = i;

    if ((uint64)value > max) max = value;

    UE_UPackage::FixKeywordConflict(str);

    str.append(" = ").append(fmt::format("{}", value));
    e.Members.push_back(str);
  }

  const char* type = nullptr;

  // I didn't see int16 yet, so I assume the engine generates only int32 and uint8:
  if (max > 256) {
    type = " : int32_t"; // I assume if enum has a negative value it is int32
  }
  else {
    type = " : uint8_t";
  }

  e.EnumName = object.GetName();
  e.CppName = "enum class " + object.GetName() + type;

  if (e.Members.size()) {
    arr.push_back(e);
  }
}

void UE_UPackage::SaveStruct(std::vector<Struct> &arr, FILE *file) {
  for (auto &s : arr) {
    fmt::print(file, "// {}\n// Size: {:#04x} (Inherited: {:#04x})\n{} {{\npublic:\n",  s.FullName, s.Size, s.Inherited, s.CppName);
    for (auto &m : s.Members) {
      if (m.isSuspectMember) {
        fmt::print(file, "\n\t// {} {}; // Bad member offset! {:#04x}({:#04x})", m.Type, m.Name, m.Offset, m.Size);
      }
      else {
        fmt::print(file, "\n\t{} {}; // {:#04x}({:#04x})", m.Type, m.Name, m.Offset, m.Size);
      }
    }
    if (s.Functions.size()) {
      fwrite("\n", 1, 1, file);
      for (auto &f : s.Functions) {
        fmt::print(file, "\n\t{}({}){}; // {} // ({}) // @ game+{:#08x}", f.CppName, f.Params, f.declareConst, f.FullName, f.Flags, f.Func - Base);
      }
    }
    fmt::print(file, "\n}};\n\n");
  }
}

void UE_UPackage::SaveStructSpacing(std::vector<Struct> &arr, FILE *file) {
  for (auto &s : arr) {
    fmt::print(file, "// {}\n// Size: {:#04x} (Inherited: {:#04x})\n{} {{\npublic:\n", s.FullName, s.Size, s.Inherited, s.CppName);
    for (auto &m : s.Members) {
      if (m.isSuspectMember) {
        fmt::print(file, "\n\t// {:69} {:60} //  Bad member offset! {:#04x}({:#04x})", m.Type, m.Name + ";", m.Offset, m.Size);
      }
      else {
        fmt::print(file, "\n\t{:69} {:60} // {:#04x}({:#04x})", m.Type, m.Name + ";", m.Offset, m.Size);
      }
    }
    if (s.Functions.size()) {
      fwrite("\n", 1, 1, file);
      for (auto &f : s.Functions) {
        fmt::print(file, "\n\t{:130} // {} // ({}) // @ game+{:#08x}", fmt::format("{}({}){};", f.CppName, f.Params, f.declareConst), f.FullName, f.Flags, f.Func - Base);
      }
    }

    fmt::print(file, "\n}};\n\n");
  }
}

void UE_UPackage::SaveEnum(std::vector<Enum> &arr, FILE *file) {
  for (auto &e : arr) {
    fmt::print(file, "// {}\n{} {{", e.FullName, e.CppName);

    auto lastIdx = e.Members.size() - 1;
    for (auto i = 0; i < lastIdx; i++) {
      auto& m = e.Members.at(i);
      fmt::print(file, "\n\t{},", m);
    }

    auto& m = e.Members.at(lastIdx);
    fmt::print(file, "\n\t{}", m);

    fmt::print(file, "\n}};\n\n");
  }
}

void UE_UPackage::Process() {
  auto &objects = Package->second;
  for (auto &object : objects) {
    if (object.IsA<UE_UClass>()) {
      GenerateStruct(object.Cast<UE_UStruct>(), Classes, FindPointers);
    } else if (object.IsA<UE_UScriptStruct>()) {
      GenerateStruct(object.Cast<UE_UStruct>(), Structures, false);
    } else if (object.IsA<UE_UEnum>()) {
      GenerateEnum(object.Cast<UE_UEnum>(), Enums);
    }
  }
}

void UE_UPackage::AddAlignDef(FILE* file, int type) {
  if (type == 1) {
    fmt::print(file, "\n#ifdef _MSC_VER\n\t#pragma pack(push, 0x01)\n#endif\n");
  }
  else if (type == 2) {
    fmt::print(file, "\n#ifdef _MSC_VER\n\t#pragma pack(pop)\n#endif\n");
  }
}

void UE_UPackage::AddNamespaceDef(FILE* file, int type) {

  if (type == 1) {
    fmt::print(file, "\nnamespace {} {{\n", GNameSpace);
  }
  else if (type == 2) {
    fmt::print(file, "\n}}\n");
  }
}

void UE_UPackage::SavePackageHeader(bool hasClassHeader, bool hasStructHeader, FILE* file) {
  fmt::print(file, "#pragma once\n\n");
  std::string packageName = this->packageName;
  char chars[] = "/\\:*?\"<>|+";
  for (auto c : chars) {
    auto pos = packageName.find(c);
    if (pos != std::string::npos) {
      packageName[pos] = '_';
    }
  }
  if (hasStructHeader) {
    fmt::print(file, "#include \"{}_struct.h\"\n", packageName);
  }
  if (hasClassHeader) {
    fmt::print(file, "#include \"{}_classes.h\"\n", packageName);
  }
  fmt::print(file, "#include \"{}_param.h\"\n", packageName);
}

void UE_UPackage::SavePackageCpp(FILE* cppFile, FILE* paramFile) {
  struct ParamStruct {
    std::string paramName;
    std::string funcName;
  };
  struct FunctionHeader {
    uint32 RVA;
    std::string name;
    std::vector<std::string> flags;
  };
  std::string gameInfo = "/**\n * Name: VAR_GAME_NAME\n * Version : VAR_GAME_VERSION\n */ \n";
  EngineHeaderExport::ReplaceVAR(gameInfo);
  fmt::print(cppFile, "{}", gameInfo);
  fmt::print(paramFile, "{}", gameInfo);
  fmt::print(paramFile, "#pragma once\n\n");
  AddAlignDef(paramFile, 1);
  fmt::print(paramFile, "\n\nnamespace {}\n{{\n", GNameSpace);
  fmt::print(cppFile, "#include \"../SDK.h\"");
  fmt::print(cppFile, "\n\nnamespace {}\n{{\n", GNameSpace);
  auto GenerateFunctionHeader = [](FILE* file, FunctionHeader &header) {
    fmt::print(file, "\t/**\n");
    fmt::print(file, "\t * Function: \n");
    fmt::print(file, "\t * \tRVA: {:#08X}\n", header.RVA);
    fmt::print(file, "\t * \tName: {}\n", header.name);
    std::string flags = "(";
    for (int i = 0; i < header.flags.size(); i++) {
      if (i != 0) {
        flags += ", " + header.flags[i];
      }
      else {
        flags += header.flags[i];
      }
    }
    flags += ")";
    fmt::print(file, "\t * \tFlags: {}\n", flags);
    fmt::print(file, "\t */\n");
  };
  auto InjectGetBoneWorldPos = [](FILE* file, Struct& stru) {
    std::string codeTemplate;
    auto replaceSubstr = [](std::string& originalStr, std::string substring, std::string replacement) {
      size_t pos = 0;
      const size_t substringLength = substring.length();
      const size_t replacementLength = replacement.length();

      while ((pos = originalStr.find(substring, pos)) != std::string::npos) {
        originalStr.replace(pos, substringLength, replacement);
        pos += replacementLength; // Move past the replaced substring
      }
    };
    if (!EngineHeaderExport::LoadResourceText(codeTemplate, INJECTED_GETBONEWORLDPOS)) return;
    replaceSubstr(codeTemplate, "\r\n", "\n");
    fmt::print(file, "\n{}\n", codeTemplate);
  };
  auto GenerateStaticClass = [&GenerateFunctionHeader, &InjectGetBoneWorldPos](FILE* file, Struct& stru) {
    if (stru.ClassName == "USkinnedMeshComponent") {
      // inject GetBoneWorldPos function
      InjectGetBoneWorldPos(file, stru);
    }
    FunctionHeader header;
    header.RVA = 0;
    header.name = fmt::format("PredefinedFunction {}.StaticClass", GetValidClassName(stru.ClassName));
    header.flags.push_back("Predefined");
    header.flags.push_back("Static");
    GenerateFunctionHeader(file, header);
    fmt::print(file, "\tUClass* {}::StaticClass()\n\t{{\n", GetValidClassName(stru.ClassName));
    fmt::print(file, "\t\tstatic UClass* ptr = nullptr;\n");
    fmt::print(file, "\t\tif (!ptr)\n");
    fmt::print(file, "\t\t\tptr = UObject::FindClass(\"{}\");\n", GetCpp_xString(stru.FullName));
    fmt::print(file, "\t\treturn ptr;\n");
    fmt::print(file, "\t}}\n\n");
  };

  auto GenerateProxyFunctionParamStruct = [](FILE* file, Function& func) {
    // 生成函数参数结构体
    if (func.FullName == "Dumper_Generated_Function") return;
    static std::unordered_map<std::string, int> paramStructNameMp;

    // 生成的结构体的名字
    func.GeneratedParamName = GetValidClassName(func.FullName);
    if (paramStructNameMp.count(func.GeneratedParamName)) {
      func.GeneratedParamName = func.GeneratedParamName + fmt::format("_Param_{}", ++paramStructNameMp[func.GeneratedParamName]);
    }
    else {
      paramStructNameMp[func.GeneratedParamName] = 1;
      func.GeneratedParamName = func.GeneratedParamName + "_Param";
    }

    // 对func的参数列表按偏移排序然后生成结构体
    std::sort(func.paramInfo.begin(), func.paramInfo.end(), [](const ParamInfo& a, const ParamInfo& b) {
      return a.Offset < b.Offset;
      });
    uint32 offset = 0;
    std::vector<Member> members;
    auto replaceSubstr = [](std::string& originalStr, std::string substring, std::string replacement) {
      size_t pos = 0;
      const size_t substringLength = substring.length();
      const size_t replacementLength = replacement.length();

      while ((pos = originalStr.find(substring, pos)) != std::string::npos) {
        originalStr.replace(pos, substringLength, replacement);
        pos += replacementLength; // Move past the replaced substring
      }
    };
    for (auto& param : func.paramInfo) {
      assert(param.Size != 0);
      assert(param.Offset >= offset);
      if (param.Offset > offset) {
        Member padding;
        padding.Type = "char";
        padding.Name = fmt::format("pad_{:0X}[{:#0x}]", offset, param.Offset - offset);
        padding.Offset = offset;
        padding.Size = param.Offset - offset;
        members.push_back(padding);
        offset += padding.Size;
      }
      Member mParam;
      mParam.Type = param.Type;
      replaceSubstr(mParam.Type, "&", "");
      mParam.Name = param.Name;
      mParam.Offset = param.Offset;
      mParam.Size = param.Size;
      mParam.isSuspectMember = false;
      offset += mParam.Size;
      members.push_back(mParam);
    }
    fmt::print(file, "\tstruct {}\n\t{{\n\tpublic:\n", func.GeneratedParamName);
    for (auto& m : members) {
      fmt::print(file, "\n\t\t{} {}; // {:#04x}({:#04x})", m.Type, m.Name, m.Offset, m.Size);
    }
    fmt::print(file, "\n\t}};\n\n");
  };
  auto GenerateProxyFunctionBody = [&GenerateFunctionHeader](FILE* file, Function& func, Struct& stru) {
    // 生成函数体
    if (func.FullName == "Dumper_Generated_Function") return;
    FunctionHeader header;
    header.RVA = func.Func - Base;
    header.name = ProcessUTF8Char(func.FullName);
    GetFlagOutVector(func.FuncFlag, header.flags);
    GenerateFunctionHeader(file, header);
    std::string ProcessedFullName = GetCpp_xString(func.FullName);
    fmt::print(file, "\t{} {}::{}({})\n\t{{\n", func.RetType, GetValidClassName(stru.ClassName), func.FuncName, func.Params);
    fmt::print(file, "\t\tstatic UFunction* fn = nullptr;\n");
    fmt::print(file, "\t\tif (!fn)\n");
    fmt::print(file, "\t\t\tfn = UObject::FindObject<UFunction>(\"{}\");\n", ProcessedFullName);
    fmt::print(file, "\t\t{} params {{ }};\n", func.GeneratedParamName);
    for (auto& param : func.paramInfo) {
      if (param.Name == "ReturnValue") continue;
      fmt::print(file, "\t\tparams.{} = {};\n", param.Name, param.Name);
    }
    fmt::print(file, "\n\t\tauto flags = fn->FunctionFlags;\n");
    if (func.FuncFlag & FUNC_Native) {
      fmt::print(file, "\t\tfn->FunctionFlags |= 0x00000400;\n");
    }
    fmt::print(file, "\t\tUObject::ProcessEvent(fn, &params);\n");
    fmt::print(file, "\t\tfn->FunctionFlags = flags;\n\n");

    for (auto& param : func.paramInfo) {
      if (param.Type[param.Type.size() - 1] == '&') {
        fmt::print(file, "\t\t{} = params.{};\n", param.Name, param.Name);
      }
    }

    if (func.RetType != "void") {
      if (func.badDeclareFunc) {
        fmt::print(file, "\t\treturn {{ }};  // BAD DECLARE FUNCTION!\n");
      }
      else {
        fmt::print(file, "\t\treturn params.ReturnValue;\n");
      }
    }
    fmt::print(file, "\t}}\n");
  };
  for (auto& stru : this->Structures) {
    GenerateStaticClass(cppFile, stru);
  }
  for (auto& stru : this->Classes) {
    GenerateStaticClass(cppFile, stru);
  }
  // 开始生成代理函数
  for (auto& stru : this->Structures) {
    for (auto& func : stru.Functions) {
      GenerateProxyFunctionParamStruct(paramFile, func);
      GenerateProxyFunctionBody(cppFile, func, stru);
    }
  }
  for (auto& stru : this->Classes) {
    for (auto& func : stru.Functions) {
      GenerateProxyFunctionParamStruct(paramFile, func);
      GenerateProxyFunctionBody(cppFile, func, stru);
    }
  }

  fmt::print(cppFile, "}}");
  fmt::print(paramFile, "}}");
  AddAlignDef(paramFile, 2);
}

bool UE_UPackage::Save(const fs::path &dir, bool spacing) {
  if (!(Classes.size() || Structures.size() || Enums.size())) {
    return false;
  }

  std::string packageName = this->packageName;

  char chars[] = "/\\:*?\"<>|+";
  for (auto c : chars) {
    auto pos = packageName.find(c);
    if (pos != std::string::npos) {
      packageName[pos] = '_';
    }
  }
  bool hasClassHeader = false;
  bool hasStructHeader = false;

  if (Classes.size()) {
    File file(dir / (packageName + "_classes.h"), "w");
    hasClassHeader = true;
    if (!file) {
      return false;
    }
    UE_UPackage::AddAlignDef(file, 1);
    UE_UPackage::AddNamespaceDef(file, 1);
    if (spacing) {
      UE_UPackage::SaveStructSpacing(Classes, file);
    } else {
      UE_UPackage::SaveStruct(Classes, file);
    }
    UE_UPackage::AddNamespaceDef(file, 2);
    UE_UPackage::AddAlignDef(file,2);
  }

  if (Structures.size() || Enums.size()) {
    File file(dir / (packageName + "_struct.h"), "w");
    hasStructHeader = true;
    if (!file) {
      return false;
    }
    UE_UPackage::AddAlignDef(file, 1);
    UE_UPackage::AddNamespaceDef(file, 1);
    if (Enums.size()) {
      UE_UPackage::SaveEnum(Enums, file);
    }

    if (Structures.size()) {
      if (spacing) {
        UE_UPackage::SaveStructSpacing(Structures, file);
      } else {
        UE_UPackage::SaveStruct(Structures, file);
      }
    }
    UE_UPackage::AddNamespaceDef(file, 2);
    UE_UPackage::AddAlignDef(file, 2);
  }
  {
    // 导出package对应的头文件
    File file(dir / (packageName + "_package.h"), "w");
    UE_UPackage::SavePackageHeader(hasClassHeader, hasStructHeader, file);
  }
  {
    // 导出代理用函数的cpp文件
    File cpp(dir / (packageName + "_package.cpp"), "w");
    File param(dir / (packageName + "_param.h"), "w");
    UE_UPackage::SavePackageCpp(cpp, param);
  }

  return true;
}

UE_UObject UE_UPackage::GetObject() const { return UE_UObject(Package->first); }


